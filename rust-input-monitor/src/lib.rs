use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use libc::{c_int, c_void};
use x11rb::connection::Connection;
use x11rb::protocol::xproto::ConnectionExt;
use x11rb::protocol::screensaver::ConnectionExt as ScreenSaverExt;
use x11rb::rust_connection::RustConnection;
use x11rb::protocol::Event;

type InputMonitorCallback = extern "C" fn(*mut c_void) -> c_int;

#[repr(C)]
pub struct InputMonitor {
    is_active: AtomicBool,
    callback: AtomicUsize, // Store as usize to make it Send
    user_data: AtomicUsize, // Store as usize to make it Send
    use_global_monitoring: AtomicBool,
    thread_handle: Mutex<Option<thread::JoinHandle<()>>>,
    stop_flag: Arc<AtomicBool>,
}

impl InputMonitor {
    fn new() -> Self {
        InputMonitor {
            is_active: AtomicBool::new(false),
            callback: AtomicUsize::new(0),
            user_data: AtomicUsize::new(0),
            use_global_monitoring: AtomicBool::new(true),
            thread_handle: Mutex::new(None),
            stop_flag: Arc::new(AtomicBool::new(false)),
        }
    }

    fn set_callback(&self, callback: InputMonitorCallback, user_data: *mut c_void) {
        self.callback.store(callback as usize, Ordering::Relaxed);
        self.user_data.store(user_data as usize, Ordering::Relaxed);
    }

    fn start(&self) {
        if self.is_active.load(Ordering::Relaxed) {
            eprintln!("Input monitor: already active, not starting again");
            return;
        }

        println!("Input monitor: starting monitoring");
        self.is_active.store(true, Ordering::Relaxed);
        self.stop_flag.store(false, Ordering::Relaxed);

        if self.use_global_monitoring.load(Ordering::Relaxed) {
            if let Err(e) = self.start_x11_monitoring() {
                eprintln!("Failed to setup global monitoring: {}, using fallback", e);
                self.use_global_monitoring.store(false, Ordering::Relaxed);
                // In real implementation, we'd fall back to window-based monitoring
                // For now, we'll just mark as inactive
                self.is_active.store(false, Ordering::Relaxed);
            } else {
                println!("Global input monitoring started successfully");
            }
        }
    }

    fn stop(&self) {
        if !self.is_active.load(Ordering::Relaxed) {
            return;
        }

        self.stop_flag.store(true, Ordering::Relaxed);
        
        // Wait for thread to finish
        if let Ok(mut handle) = self.thread_handle.lock() {
            if let Some(thread) = handle.take() {
                let _ = thread.join();
            }
        }

        self.is_active.store(false, Ordering::Relaxed);
    }

    fn start_x11_monitoring(&self) -> Result<(), Box<dyn std::error::Error>> {
        let stop_flag = Arc::clone(&self.stop_flag);
        let callback_ptr = self.callback.load(Ordering::Relaxed);
        let user_data = self.user_data.load(Ordering::Relaxed);

        let thread = thread::spawn(move || {
            if let Err(e) = monitor_x11_input(stop_flag, callback_ptr as *mut c_void, user_data as *mut c_void) {
                eprintln!("X11 monitoring error: {}", e);
            }
        });

        let mut handle = self.thread_handle.lock().unwrap();
        *handle = Some(thread);

        Ok(())
    }

    fn is_active(&self) -> bool {
        self.is_active.load(Ordering::Relaxed)
    }
}

fn monitor_x11_input(
    stop_flag: Arc<AtomicBool>,
    callback_ptr: *mut c_void,
    user_data: *mut c_void,
) -> Result<(), Box<dyn std::error::Error>> {
    // Add a small delay before starting to monitor
    // This prevents catching events from the break overlay dismissal
    thread::sleep(Duration::from_millis(500));
    
    // Connect to X11
    let (conn, screen_num) = RustConnection::connect(None)?;
    let screen = &conn.setup().roots[screen_num];
    let root = screen.root;

    // Use regular X11 events instead of XInput2 for simplicity
    // This will work for most use cases
    use x11rb::protocol::xproto::{EventMask, ChangeWindowAttributesAux};
    
    let event_mask = EventMask::KEY_PRESS | EventMask::BUTTON_PRESS | EventMask::POINTER_MOTION;
    let aux = ChangeWindowAttributesAux::new().event_mask(event_mask);
    conn.change_window_attributes(root, &aux)?;
    conn.flush()?;

    // Motion tracking for threshold
    let mut last_x = 0.0;
    let mut last_y = 0.0;

    while !stop_flag.load(Ordering::Relaxed) {
        match conn.poll_for_event() {
            Ok(Some(event)) => {
                // For simplicity, detect any key or button events as activity
                // XInput2 raw events are complex to parse from generic events
                // We'll use a simpler approach for now
                let activity_detected = match &event {
                    Event::KeyPress(_) => {
                        println!("X11 input monitor: key press detected");
                        true
                    }
                    Event::ButtonPress(_) => {
                        println!("X11 input monitor: button press detected");
                        true
                    }
                    Event::MotionNotify(motion) => {
                        // Check for significant motion
                        let curr_x = motion.event_x as f64;
                        let curr_y = motion.event_y as f64;
                        
                        let dx = curr_x - last_x;
                        let dy = curr_y - last_y;
                        let distance_sq = dx * dx + dy * dy;
                        
                        // Only trigger on movements > 10 pixels
                        if distance_sq >= 100.0 {
                            last_x = curr_x;
                            last_y = curr_y;
                            true
                        } else {
                            false
                        }
                    }
                    _ => false,
                };

                if activity_detected {
                    println!("X11 input monitor: activity detected! Triggering callback");
                    
                    // Trigger callback
                    if !callback_ptr.is_null() {
                        unsafe {
                            let callback: InputMonitorCallback = std::mem::transmute(callback_ptr);
                            callback(user_data);
                        }
                    }
                    
                    // Stop monitoring
                    return Ok(());
                }
            }
            Ok(None) => {
                // No event available, sleep briefly
                thread::sleep(Duration::from_millis(10));
            }
            Err(e) => {
                eprintln!("Error polling for events: {}", e);
                break;
            }
        }
    }

    Ok(())
}

fn get_idle_time() -> Result<i32, Box<dyn std::error::Error>> {
    let (conn, screen_num) = RustConnection::connect(None)?;
    let screen = &conn.setup().roots[screen_num];
    let root = screen.root;

    // Query screensaver extension
    let version = conn.screensaver_query_version(1, 0)?;
    let _reply = version.reply()?;

    // Query idle info
    let info = conn.screensaver_query_info(root)?;
    let reply = info.reply()?;

    // Convert milliseconds to seconds
    Ok((reply.ms_since_user_input / 1000) as i32)
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
        let monitor = Box::from_raw(monitor);
        monitor.stop(); // Ensure thread is stopped before dropping
        drop(monitor);
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_set_callback(
    monitor: *mut InputMonitor,
    callback: InputMonitorCallback,
    user_data: *mut c_void,
) {
    if monitor.is_null() {
        return;
    }
    unsafe {
        (*monitor).set_callback(callback, user_data);
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_set_window(
    monitor: *mut InputMonitor,
    _window: *mut c_void,
) {
    // This is for GTK window-based monitoring fallback
    // We're not implementing this in the Rust version yet
    if monitor.is_null() {
        return;
    }
    // For now, just acknowledge the call
    println!("Input monitor: set_window called (not implemented in Rust version)");
}

#[no_mangle]
pub extern "C" fn input_monitor_start(monitor: *mut InputMonitor) {
    if monitor.is_null() {
        eprintln!("Input monitor: start called with NULL monitor");
        return;
    }
    unsafe {
        (*monitor).start();
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_stop(monitor: *mut InputMonitor) {
    if monitor.is_null() {
        return;
    }
    unsafe {
        (*monitor).stop();
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_is_active(monitor: *mut InputMonitor) -> c_int {
    if monitor.is_null() {
        return 0;
    }
    unsafe {
        if (*monitor).is_active() { 1 } else { 0 }
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_get_idle_time(_monitor: *mut InputMonitor) -> c_int {
    match get_idle_time() {
        Ok(seconds) => seconds,
        Err(e) => {
            eprintln!("Failed to get idle time: {}", e);
            -1
        }
    }
}