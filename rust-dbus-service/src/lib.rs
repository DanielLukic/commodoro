use std::os::raw::c_void;
use std::sync::Mutex;
use once_cell::sync::Lazy;

use gio::{DBusConnection, DBusMethodInvocation, DBusNodeInfo, BusNameOwnerFlags, BusType, OwnerId};
use glib::Variant;

// Store app pointer for callbacks (using usize to make it Send+Sync)
static APP_POINTER: Lazy<Mutex<Option<usize>>> = Lazy::new(|| Mutex::new(None));

// External C functions we need to call
extern "C" {
    fn on_start_clicked(widget: *mut c_void, app: *mut c_void);
    fn on_reset_clicked(widget: *mut c_void, app: *mut c_void);
    fn timer_skip_phase(timer: *mut c_void);
    fn timer_get_state(timer: *mut c_void) -> u32; // TimerState enum as u32
    fn gtk_widget_get_visible(widget: *mut c_void) -> i32;
    fn gtk_widget_hide(widget: *mut c_void);
    fn gtk_widget_show(widget: *mut c_void);
    fn gtk_window_present(window: *mut c_void);
}

// GomodaroApp struct layout for accessing members
#[repr(C)]
struct GomodaroApp {
    window: *mut c_void,
    timer: *mut c_void,
    // ... other fields we don't need to access
}

pub struct DBusService {
    owner_id: Option<OwnerId>,
    connection: Option<DBusConnection>,
}

impl DBusService {
    fn new(app_pointer: *mut c_void) -> Self {
        // Store app pointer for use in callbacks
        if let Ok(mut guard) = APP_POINTER.lock() {
            *guard = Some(app_pointer as usize);
        }
        
        Self {
            owner_id: None,
            connection: None,
        }
    }
    
    fn publish(&mut self) {
        let owner_id = gio::bus_own_name(
            BusType::Session,
            "org.dl.commodoro",
            BusNameOwnerFlags::NONE,
            move |_connection, name| {
                // on_bus_acquired callback
                println!("Bus acquired: {}", name);
            },
            move |connection, name| {
                // on_name_acquired callback
                Self::register_dbus_object(&connection, name);
            },
            move |_connection, _name| {
                // on_name_lost callback
                eprintln!("Commodoro is already running. Use 'commodoro show_hide' to show the window.");
                std::process::exit(1);
            },
        );
        
        self.owner_id = Some(owner_id);
    }
    
    fn unpublish(&mut self) {
        if let Some(owner_id) = self.owner_id.take() {
            gio::bus_unown_name(owner_id);
        }
    }
    
    fn register_dbus_object(connection: &DBusConnection, _name: &str) {
        let introspection_xml = r#"
            <node>
              <interface name='org.dl.commodoro.Timer'>
                <method name='ToggleTimer'/>
                <method name='ResetTimer'/>
                <method name='ToggleBreak'/>
                <method name='ShowHide'/>
                <method name='GetState'>
                  <arg type='s' name='state' direction='out'/>
                </method>
              </interface>
            </node>
        "#;
        
        let node_info = DBusNodeInfo::for_xml(introspection_xml)
            .expect("Failed to create introspection data");
        
        let interface_info = node_info
            .lookup_interface("org.dl.commodoro.Timer")
            .expect("Failed to find interface");
        
        let result = connection.register_object(
            "/org/dl/commodoro",
            &interface_info,
            move |connection, sender, object_path, interface_name, method_name, parameters, invocation| {
                Self::handle_method_call(
                    &connection,
                    sender,
                    object_path,
                    interface_name,
                    method_name,
                    &parameters,
                    &invocation,
                );
            },
            move |_connection, _sender, _object_path, _interface_name, _property_name| {
                // get_property callback - not implemented
                Variant::from("")
            },
            move |_connection, _sender, _object_path, _interface_name, _property_name, _value| {
                // set_property callback - not implemented
                false
            },
        );
        
        if let Err(error) = result {
            eprintln!("Failed to register D-Bus object: {}", error);
        }
    }
    
    fn handle_method_call(
        _connection: &DBusConnection,
        _sender: &str,
        _object_path: &str,
        _interface_name: &str,
        method_name: &str,
        _parameters: &Variant,
        invocation: &DBusMethodInvocation,
    ) {
        // Get app pointer
        let app_ptr = {
            if let Ok(guard) = APP_POINTER.lock() {
                *guard
            } else {
                None
            }
        };
        
        let app_ptr = match app_ptr {
            Some(ptr) => ptr as *mut c_void,
            None => {
                invocation.clone().return_dbus_error(
                    "org.freedesktop.DBus.Error.Failed",
                    "App pointer not available",
                );
                return;
            }
        };
        
        unsafe {
            let app = app_ptr as *mut GomodaroApp;
            
            match method_name {
                "ToggleTimer" => {
                    on_start_clicked(std::ptr::null_mut(), app_ptr);
                    invocation.clone().return_value(None);
                }
                "ResetTimer" => {
                    on_reset_clicked(std::ptr::null_mut(), app_ptr);
                    invocation.clone().return_value(None);
                }
                "ToggleBreak" => {
                    timer_skip_phase((*app).timer);
                    invocation.clone().return_value(None);
                }
                "ShowHide" => {
                    let visible = gtk_widget_get_visible((*app).window) != 0;
                    if visible {
                        gtk_widget_hide((*app).window);
                    } else {
                        gtk_widget_show((*app).window);
                        gtk_window_present((*app).window);
                    }
                    invocation.clone().return_value(None);
                }
                "GetState" => {
                    let state = timer_get_state((*app).timer);
                    let state_str = match state {
                        0 => "IDLE",
                        1 => "WORK", 
                        2 => "SHORT_BREAK",
                        3 => "LONG_BREAK",
                        4 => "PAUSED",
                        _ => "UNKNOWN",
                    };
                    
                    let result = Variant::from((state_str,));
                    invocation.clone().return_value(Some(&result));
                }
                _ => {
                    invocation.clone().return_dbus_error(
                        "org.freedesktop.DBus.Error.UnknownMethod",
                        "Method does not exist",
                    );
                }
            }
        }
    }
}

impl Drop for DBusService {
    fn drop(&mut self) {
        self.unpublish();
    }
}

// C FFI exports
#[no_mangle]
pub extern "C" fn dbus_service_new(app_pointer: *mut c_void) -> *mut DBusService {
    Box::into_raw(Box::new(DBusService::new(app_pointer)))
}

#[no_mangle]
pub extern "C" fn dbus_service_free(service: *mut DBusService) {
    if service.is_null() {
        return;
    }
    
    unsafe {
        let service = Box::from_raw(service);
        drop(service);
    }
}

#[no_mangle]
pub extern "C" fn dbus_service_publish(service: *mut DBusService) {
    if service.is_null() {
        return;
    }
    
    unsafe {
        let service = &mut *service;
        service.publish();
    }
}

#[no_mangle]
pub extern "C" fn dbus_service_unpublish(service: *mut DBusService) {
    if service.is_null() {
        return;
    }
    
    unsafe {
        let service = &mut *service;
        service.unpublish();
    }
}