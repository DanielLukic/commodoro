use std::os::raw::{c_int, c_void};
use std::ptr;
use std::sync::{Arc, Mutex};
use std::time::Duration;
use glib::{source::timeout_add, SourceId};
use x11::xlib::{XCloseDisplay, XOpenDisplay, XDefaultRootWindow};
use x11::xss::{XScreenSaverAllocInfo, XScreenSaverQueryExtension, XScreenSaverQueryInfo};
use once_cell::sync::Lazy;

// Callback type matching the C interface
pub type InputMonitorCallback = extern "C" fn(*mut c_void);

// Global state for the timer callback (workaround for thread safety)
static GLOBAL_MONITOR_STATE: Lazy<Mutex<Option<(InputMonitorCallback, usize, bool)>>> = 
    Lazy::new(|| Mutex::new(None));

static GLOBAL_LAST_IDLE_TIME: Lazy<Mutex<i32>> = Lazy::new(|| Mutex::new(0));

pub struct InputMonitor {
    is_active: bool,
    callback: Option<InputMonitorCallback>,
    user_data: *mut c_void,
    
    // Simplified idle monitoring
    idle_timer_id: Option<SourceId>,
    activity_threshold: i32,  // seconds of inactivity before considering idle
}

impl InputMonitor {
    fn new() -> Self {
        Self {
            is_active: false,
            callback: None,
            user_data: ptr::null_mut(),
            idle_timer_id: None,
            activity_threshold: 2,  // 2 seconds of activity triggers callback
        }
    }

    fn set_callback(&mut self, callback: Option<InputMonitorCallback>, user_data: *mut c_void) {
        self.callback = callback;
        self.user_data = user_data;
    }

    fn start(&mut self) {
        if self.is_active {
            println!("Input monitor: already active, not starting again");
            return;
        }

        println!("Input monitor: starting idle monitoring (checking every 500ms)");
        self.is_active = true;
        
        // Store state globally for the timer callback
        if let Some(callback) = self.callback {
            if let Ok(mut state) = GLOBAL_MONITOR_STATE.lock() {
                *state = Some((callback, self.user_data as usize, true));
            }
        }
        
        // Initialize last idle time
        let initial_idle_time = get_idle_time();
        if let Ok(mut last_idle) = GLOBAL_LAST_IDLE_TIME.lock() {
            *last_idle = initial_idle_time;
        }

        // Check for activity every 500ms
        let source_id = timeout_add(Duration::from_millis(500), || {
            check_activity_timeout_global()
        });
        
        self.idle_timer_id = Some(source_id);
    }

    fn stop(&mut self) {
        if !self.is_active {
            return;
        }

        println!("Input monitor: stopping monitoring");

        // Remove the timer
        if let Some(timer_id) = self.idle_timer_id.take() {
            timer_id.remove();
        }

        // Clear global state
        if let Ok(mut state) = GLOBAL_MONITOR_STATE.lock() {
            if let Some((callback, user_data, _)) = *state {
                *state = Some((callback, user_data, false));
            }
        }

        self.is_active = false;
    }

    fn is_active(&self) -> bool {
        self.is_active
    }

    fn get_idle_time(&self) -> c_int {
        get_idle_time()
    }
}

fn check_activity_timeout_global() -> glib::ControlFlow {
    // Check if monitoring is still active
    let (callback, user_data, is_active) = {
        if let Ok(state_guard) = GLOBAL_MONITOR_STATE.lock() {
            if let Some((callback, user_data, is_active)) = *state_guard {
                (Some(callback), user_data, is_active)
            } else {
                return glib::ControlFlow::Break;
            }
        } else {
            return glib::ControlFlow::Break;
        }
    };

    if !is_active {
        return glib::ControlFlow::Break;
    }

    let current_idle_time = get_idle_time();

    // If we can't get idle time, continue checking
    if current_idle_time < 0 {
        return glib::ControlFlow::Continue;
    }

    let last_idle_time = {
        if let Ok(last_idle_guard) = GLOBAL_LAST_IDLE_TIME.lock() {
            *last_idle_guard
        } else {
            return glib::ControlFlow::Continue;
        }
    };

    // Check if user became active (idle time decreased significantly)
    if current_idle_time < last_idle_time - 1 {
        println!("Input monitor: activity detected! (idle time: {} -> {})", 
                last_idle_time, current_idle_time);

        // Stop monitoring and trigger callback
        if let Ok(mut state) = GLOBAL_MONITOR_STATE.lock() {
            if let Some((cb, ud, _)) = *state {
                *state = Some((cb, ud, false));
            }
        }

        if let Some(callback) = callback {
            // Schedule callback on main thread
            glib::idle_add(move || {
                println!("Input monitor: trigger_callback_idle called, is_active=0");
                callback(user_data as *mut c_void);
                glib::ControlFlow::Break
            });
        }

        return glib::ControlFlow::Break;
    }

    // Update last idle time
    if let Ok(mut last_idle) = GLOBAL_LAST_IDLE_TIME.lock() {
        *last_idle = current_idle_time;
    }
    
    glib::ControlFlow::Continue
}

fn get_idle_time() -> c_int {
    unsafe {
        let display = XOpenDisplay(ptr::null());
        if display.is_null() {
            eprintln!("Failed to open X11 display for idle time detection");
            return -1;
        }

        let mut event_base = 0;
        let mut error_base = 0;
        if XScreenSaverQueryExtension(display, &mut event_base, &mut error_base) == 0 {
            eprintln!("XScreenSaver extension not available");
            XCloseDisplay(display);
            return -1;
        }

        let info = XScreenSaverAllocInfo();
        if info.is_null() {
            eprintln!("Failed to allocate XScreenSaverInfo");
            XCloseDisplay(display);
            return -1;
        }

        if XScreenSaverQueryInfo(display, XDefaultRootWindow(display), info) == 0 {
            eprintln!("Failed to query idle time");
            libc::free(info as *mut libc::c_void);
            XCloseDisplay(display);
            return -1;
        }

        let idle_seconds = ((*info).idle / 1000) as c_int;  // Convert milliseconds to seconds

        libc::free(info as *mut libc::c_void);
        XCloseDisplay(display);

        idle_seconds
    }
}

// C FFI exports
#[no_mangle]
pub extern "C" fn input_monitor_new() -> *mut InputMonitor {
    Box::into_raw(Box::new(InputMonitor::new()))
}

#[no_mangle]
pub extern "C" fn input_monitor_free(monitor: *mut InputMonitor) {
    if monitor.is_null() {
        return;
    }
    
    unsafe {
        let mut monitor = Box::from_raw(monitor);
        monitor.stop();
        drop(monitor);
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_set_callback(
    monitor: *mut InputMonitor,
    callback: Option<InputMonitorCallback>,
    user_data: *mut c_void,
) {
    if monitor.is_null() {
        return;
    }
    
    unsafe {
        let monitor = &mut *monitor;
        monitor.set_callback(callback, user_data);
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_set_window(
    _monitor: *mut InputMonitor,
    _window: *mut c_void,
) {
    // No longer needed with idle time polling approach
    // Keeping for API compatibility
}

#[no_mangle]
pub extern "C" fn input_monitor_start(monitor: *mut InputMonitor) {
    if monitor.is_null() {
        return;
    }
    
    unsafe {
        let monitor = &mut *monitor;
        monitor.start();
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_stop(monitor: *mut InputMonitor) {
    if monitor.is_null() {
        return;
    }
    
    unsafe {
        let monitor = &mut *monitor;
        monitor.stop();
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_is_active(monitor: *mut InputMonitor) -> c_int {
    if monitor.is_null() {
        return 0;
    }
    
    unsafe {
        let monitor = &*monitor;
        if monitor.is_active() { 1 } else { 0 }
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_get_idle_time(monitor: *mut InputMonitor) -> c_int {
    if monitor.is_null() {
        return -1;
    }
    
    unsafe {
        let monitor = &*monitor;
        monitor.get_idle_time()
    }
}