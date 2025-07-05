use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};
use zbus::blocking::Connection;

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum DBusCommandResult {
    Success = 0,
    Error = 1,
    NotRunning = 2,
    StartNeeded = 3,
}

const DBUS_SERVICE_NAME: &str = "org.dl.commodoro";
const DBUS_OBJECT_PATH: &str = "/org/dl/commodoro";
const DBUS_INTERFACE_NAME: &str = "org.dl.commodoro.Timer";

/// Parse command string and return the D-Bus method name
#[no_mangle]
pub extern "C" fn dbus_parse_command(str: *const c_char) -> *const c_char {
    if str.is_null() {
        return std::ptr::null();
    }
    
    let command = unsafe {
        match CStr::from_ptr(str).to_str() {
            Ok(s) => s,
            Err(_) => return std::ptr::null(),
        }
    };
    
    let method_name = match command {
        "toggle_timer" => "ToggleTimer",
        "reset_timer" => "ResetTimer",
        "toggle_break" => "ToggleBreak",
        "show_hide" => "ShowHide",
        _ => return std::ptr::null(),
    };
    
    // Return a static string that doesn't need to be freed
    match CString::new(method_name) {
        Ok(cstring) => cstring.into_raw(),
        Err(_) => std::ptr::null(),
    }
}

/// Send a command via D-Bus to the running Commodoro instance
#[no_mangle]
pub extern "C" fn dbus_send_command(
    command: *const c_char,
    auto_start: c_int,
    _unused: *mut std::ffi::c_void,
) -> DBusCommandResult {
    if command.is_null() {
        return DBusCommandResult::Error;
    }
    
    let command_str = unsafe {
        match CStr::from_ptr(command).to_str() {
            Ok(s) => s,
            Err(_) => return DBusCommandResult::Error,
        }
    };
    
    // Get D-Bus connection
    let connection = match Connection::session() {
        Ok(conn) => conn,
        Err(e) => {
            eprintln!("Could not connect to D-Bus: {}", e);
            return DBusCommandResult::Error;
        }
    };
    
    // Try to call the method
    let result = connection.call_method(
        Some(DBUS_SERVICE_NAME),
        DBUS_OBJECT_PATH,
        Some(DBUS_INTERFACE_NAME),
        command_str,
        &(),
    );
    
    match result {
        Ok(_) => DBusCommandResult::Success,
        Err(e) => {
            // Check if the service is not running
            if e.to_string().contains("org.freedesktop.DBus.Error.ServiceUnknown") {
                if auto_start != 0 {
                    println!("Commodoro is not running.");
                    DBusCommandResult::StartNeeded
                } else {
                    eprintln!("Commodoro is not running. Use --auto-start to launch it.");
                    DBusCommandResult::NotRunning
                }
            } else {
                eprintln!("D-Bus error: {}", e);
                DBusCommandResult::Error
            }
        }
    }
}

/// Free a string returned by dbus_parse_command
#[no_mangle]
pub extern "C" fn dbus_free_string(str: *mut c_char) {
    if !str.is_null() {
        unsafe {
            let _ = CString::from_raw(str);
        }
    }
}
