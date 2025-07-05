use std::os::raw::{c_int, c_void};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use glib::MainContext;
use evdev::{Device, EventType, InputEvent};
use once_cell::sync::Lazy;

// Global state for monitoring
static MONITOR_STATE: Lazy<Mutex<Option<Arc<InputMonitorState>>>> = Lazy::new(|| Mutex::new(None));

// Callback type matching the C interface
pub type InputMonitorCallback = extern "C" fn(*mut c_void);

struct InputMonitorState {
    callback: Option<InputMonitorCallback>,
    user_data: *mut c_void,
    is_active: bool,
}

unsafe impl Send for InputMonitorState {}
unsafe impl Sync for InputMonitorState {}

pub struct InputMonitor {
    // Just a placeholder - actual state is in global MONITOR_STATE
    _marker: std::marker::PhantomData<()>,
}

impl InputMonitor {
    fn new() -> Self {
        Self {
            _marker: std::marker::PhantomData,
        }
    }

    fn set_callback(&self, callback: Option<InputMonitorCallback>, user_data: *mut c_void) {
        if let Ok(mut state) = MONITOR_STATE.lock() {
            if let Some(ref mut monitor_state) = *state {
                let new_state = Arc::new(InputMonitorState {
                    callback,
                    user_data,
                    is_active: monitor_state.is_active,
                });
                *state = Some(new_state);
            } else {
                *state = Some(Arc::new(InputMonitorState {
                    callback,
                    user_data,
                    is_active: false,
                }));
            }
        }
    }

    fn set_window(&self, _window: *mut c_void) {
        // For evdev monitoring, we don't need the window reference
        // This is just for API compatibility
    }

    fn start(&self) {
        if let Ok(mut state_guard) = MONITOR_STATE.lock() {
            if let Some(ref state) = *state_guard {
                if state.is_active {
                    println!("Input monitor: already active, not starting again");
                    return;
                }
            }

            println!("Input monitor: starting modern evdev-based monitoring");
            
            // Update state to active
            if let Some(ref monitor_state) = *state_guard {
                let new_state = Arc::new(InputMonitorState {
                    callback: monitor_state.callback,
                    user_data: monitor_state.user_data,
                    is_active: true,
                });
                let state_clone = new_state.clone();
                *state_guard = Some(new_state);
                
                // Spawn monitoring task
                thread::spawn(move || {
                    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
                    rt.block_on(async {
                        start_evdev_monitoring(state_clone).await;
                    });
                });
            }
        }
    }

    fn stop(&self) {
        if let Ok(mut state_guard) = MONITOR_STATE.lock() {
            if let Some(ref monitor_state) = *state_guard {
                // Create new state with is_active = false
                let new_state = Arc::new(InputMonitorState {
                    callback: monitor_state.callback,
                    user_data: monitor_state.user_data,
                    is_active: false,
                });
                *state_guard = Some(new_state);
                println!("Input monitor: stopped");
            }
        }
    }

    fn is_active(&self) -> bool {
        if let Ok(state_guard) = MONITOR_STATE.lock() {
            if let Some(ref state) = *state_guard {
                return state.is_active;
            }
        }
        false
    }

    fn get_idle_time(&self) -> c_int {
        // Simple fallback idle time detection
        match get_simple_idle_time() {
            Ok(idle_seconds) => idle_seconds as c_int,
            Err(e) => {
                eprintln!("Failed to get idle time: {}", e);
                -1
            }
        }
    }
}

// Modern evdev-based input monitoring
async fn start_evdev_monitoring(state: Arc<InputMonitorState>) {
    println!("Starting evdev input monitoring...");
    
    // Try to find input devices
    let devices = match find_input_devices() {
        Ok(devices) => devices,
        Err(e) => {
            eprintln!("Failed to find input devices: {}", e);
            return;
        }
    };

    if devices.is_empty() {
        eprintln!("No accessible input devices found. You may need to run as root or add user to 'input' group.");
        return;
    }

    println!("Found {} input devices to monitor", devices.len());

    // Monitor all devices concurrently
    let mut streams = Vec::new();
    for mut device in devices {
        // evdev 0.12 doesn't have into_event_stream, we'll poll directly
        streams.push(device);
    }

    if streams.is_empty() {
        eprintln!("No input devices available");
        return;
    }

    // Monitor for input events using polling
    while state.is_active {
        let mut found_activity = false;
        
        for device in &mut streams {
            // Try to read events from each device
            if let Ok(events) = device.fetch_events() {
                for event in events {
                    if is_user_activity_event(&event) {
                        println!("User activity detected via evdev!");
                        trigger_callback_from_async(state.clone());
                        found_activity = true;
                        break;
                    }
                }
                if found_activity {
                    break;
                }
            }
        }
        
        if found_activity {
            break;
        }
        
        // Small delay to avoid busy waiting
        tokio::time::sleep(Duration::from_millis(50)).await;
    }
    
    println!("Evdev monitoring stopped");
}

fn find_input_devices() -> Result<Vec<Device>, Box<dyn std::error::Error>> {
    let mut devices = Vec::new();
    
    // Try to open common input device paths
    for entry in std::fs::read_dir("/dev/input")? {
        let entry = entry?;
        let path = entry.path();
        
        if let Some(filename) = path.file_name().and_then(|n| n.to_str()) {
            if filename.starts_with("event") {
                match Device::open(&path) {
                    Ok(device) => {
                        // Check if device has keyboard or mouse capabilities
                        if device.supported_events().contains(EventType::KEY) ||
                           device.supported_events().contains(EventType::RELATIVE) ||
                           device.supported_events().contains(EventType::ABSOLUTE) {
                            println!("Opened input device: {} ({})", 
                                   device.name().unwrap_or("Unknown"), 
                                   path.display());
                            devices.push(device);
                        }
                    }
                    Err(e) => {
                        // This is expected for devices we don't have permission to access
                        if e.kind() != std::io::ErrorKind::PermissionDenied {
                            eprintln!("Failed to open {}: {}", path.display(), e);
                        }
                    }
                }
            }
        }
    }
    
    Ok(devices)
}

fn is_user_activity_event(event: &InputEvent) -> bool {
    match event.event_type() {
        EventType::KEY => {
            // Key press events (excluding key release)
            event.value() == 1
        }
        EventType::RELATIVE => {
            // Mouse movement (check for significant movement)
            if event.value().abs() > 2 { // Filter out tiny movements
                true
            } else {
                false
            }
        }
        EventType::ABSOLUTE => {
            // Absolute positioning (touchpad, touchscreen)
            true
        }
        _ => false,
    }
}

fn trigger_callback_from_async(state: Arc<InputMonitorState>) {
    if let Some(callback) = state.callback {
        let user_data = state.user_data;
        
        // Schedule callback on main thread using glib
        let main_context = MainContext::default();
        main_context.spawn_local(async move {
            println!("Triggering input monitor callback from async context");
            callback(user_data);
        });
    }
}

// Simple idle time detection fallback
fn get_simple_idle_time() -> Result<u64, Box<dyn std::error::Error>> {
    // Try to get load average as a rough proxy for system activity
    // This is a very basic fallback implementation
    
    let loadavg_content = std::fs::read_to_string("/proc/loadavg")?;
    let parts: Vec<&str> = loadavg_content.trim().split_whitespace().collect();
    if parts.len() >= 1 {
        let load: f64 = parts[0].parse()?;
        // Very rough heuristic: if load is low, assume system has been idle
        if load < 0.1 {
            return Ok(60); // Assume 60 seconds of idle time
        } else {
            return Ok(0); // System appears active
        }
    }
    
    Err("Could not determine idle time".into())
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
        let monitor = &*monitor;
        monitor.set_callback(callback, user_data);
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_set_window(
    monitor: *mut InputMonitor,
    window: *mut c_void,
) {
    if monitor.is_null() {
        return;
    }
    
    unsafe {
        let monitor = &*monitor;
        monitor.set_window(window);
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_start(monitor: *mut InputMonitor) {
    if monitor.is_null() {
        return;
    }
    
    unsafe {
        let monitor = &*monitor;
        monitor.start();
    }
}

#[no_mangle]
pub extern "C" fn input_monitor_stop(monitor: *mut InputMonitor) {
    if monitor.is_null() {
        return;
    }
    
    unsafe {
        let monitor = &*monitor;
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