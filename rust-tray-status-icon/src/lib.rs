use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_void};
use std::sync::Mutex;
use once_cell::sync::Lazy;
use gdk_pixbuf::Pixbuf;

// Use raw GTK sys calls for StatusIcon since it's deprecated and not in gtk-rs 0.18
use gtk_sys::{
    GtkStatusIcon, gtk_status_icon_new, gtk_status_icon_set_title,
    gtk_status_icon_set_tooltip_text, gtk_status_icon_set_visible,
    gtk_status_icon_set_from_icon_name, gtk_status_icon_is_embedded,
};
use glib_sys::{GFALSE, GTRUE};
use gobject_sys::{g_object_unref, g_signal_connect_data};

// Store callbacks in a global mutex since GTK callbacks can't capture arbitrary data
static CALLBACK_STORE: Lazy<Mutex<Option<(TrayStatusIconCallback, usize)>>> = 
    Lazy::new(|| Mutex::new(None));

pub type TrayStatusIconCallback = extern "C" fn(*const c_char, *mut c_void);

pub struct TrayStatusIcon {
    status_icon: *mut GtkStatusIcon,
}

unsafe impl Send for TrayStatusIcon {}
unsafe impl Sync for TrayStatusIcon {}

// Signal handler wrapper functions
extern "C" fn on_activate_wrapper(_status_icon: *mut GtkStatusIcon, _user_data: *mut c_void) {
    trigger_callback("activate");
}

extern "C" fn on_popup_menu_wrapper(
    _status_icon: *mut GtkStatusIcon, 
    _button: u32, 
    _activate_time: u32, 
    _user_data: *mut c_void
) {
    trigger_callback("popup-menu");
}

impl TrayStatusIcon {
    fn new() -> Self {
        unsafe {
            // Create GTK3 status icon using raw sys calls
            let status_icon = gtk_status_icon_new();
            
            // Set default properties
            let title = CString::new("Commodoro").unwrap();
            gtk_status_icon_set_title(status_icon, title.as_ptr());
            
            let tooltip = CString::new("Commodoro Timer").unwrap();
            gtk_status_icon_set_tooltip_text(status_icon, tooltip.as_ptr());
            
            // Try using a standard icon first to see if StatusIcon works at all
            let icon_name = CString::new("applications-games").unwrap();
            gtk_status_icon_set_from_icon_name(status_icon, icon_name.as_ptr());
            
            gtk_status_icon_set_visible(status_icon, GTRUE);
            
            // Connect signals using raw glib calls
            let activate_signal = CString::new("activate").unwrap();
            g_signal_connect_data(
                status_icon as *mut _,
                activate_signal.as_ptr(),
                Some(std::mem::transmute(on_activate_wrapper as *const ())),
                std::ptr::null_mut(),
                None,
                0,
            );
            
            let popup_signal = CString::new("popup-menu").unwrap();
            g_signal_connect_data(
                status_icon as *mut _,
                popup_signal.as_ptr(),
                Some(std::mem::transmute(on_popup_menu_wrapper as *const ())),
                std::ptr::null_mut(),
                None,
                0,
            );
            
            TrayStatusIcon {
                status_icon,
            }
        }
    }
    
    fn update(&self, surface: *mut cairo_sys::cairo_surface_t, tooltip: Option<&str>) {
        // For now, let's skip the complex pixbuf conversion and just update tooltip
        // We can add the custom icon later once we confirm the basic mechanism works
        
        // Update tooltip
        if let Some(tooltip_text) = tooltip {
            unsafe {
                let tooltip_cstr = CString::new(tooltip_text).unwrap();
                gtk_status_icon_set_tooltip_text(self.status_icon, tooltip_cstr.as_ptr());
            }
        }
    }
    
    fn set_visible(&self, visible: bool) {
        unsafe {
            gtk_status_icon_set_visible(self.status_icon, if visible { GTRUE } else { GFALSE });
        }
    }
    
    fn is_embedded(&self) -> bool {
        unsafe {
            gtk_status_icon_is_embedded(self.status_icon) != GFALSE
        }
    }
}

impl Drop for TrayStatusIcon {
    fn drop(&mut self) {
        unsafe {
            if !self.status_icon.is_null() {
                g_object_unref(self.status_icon as *mut _);
            }
        }
    }
}

fn trigger_callback(action: &str) {
    if let Ok(guard) = CALLBACK_STORE.lock() {
        if let Some((callback, user_data)) = *guard {
            let c_action = CString::new(action).unwrap();
            callback(c_action.as_ptr(), user_data as *mut c_void);
        }
    }
}

// C FFI exports
#[no_mangle]
pub extern "C" fn tray_status_icon_new() -> *mut TrayStatusIcon {
    gtk::init().expect("Failed to initialize GTK");
    Box::into_raw(Box::new(TrayStatusIcon::new()))
}

#[no_mangle]
pub extern "C" fn tray_status_icon_free(tray: *mut TrayStatusIcon) {
    if tray.is_null() {
        return;
    }
    
    unsafe {
        let tray = Box::from_raw(tray);
        drop(tray);
    }
}

#[no_mangle]
pub extern "C" fn tray_status_icon_set_callback(
    _tray: *mut TrayStatusIcon,
    callback: Option<TrayStatusIconCallback>,
    user_data: *mut c_void,
) {
    if let Ok(mut guard) = CALLBACK_STORE.lock() {
        if let Some(callback) = callback {
            *guard = Some((callback, user_data as usize));
        } else {
            *guard = None;
        }
    }
}

#[no_mangle]
pub extern "C" fn tray_status_icon_update(
    tray: *mut TrayStatusIcon,
    surface: *mut cairo_sys::cairo_surface_t,
    tooltip: *const c_char,
) {
    if tray.is_null() {
        return;
    }
    
    unsafe {
        let tray = &*tray;
        let tooltip_str = if tooltip.is_null() {
            None
        } else {
            std::ffi::CStr::from_ptr(tooltip).to_str().ok()
        };
        
        tray.update(surface, tooltip_str);
    }
}

#[no_mangle]
pub extern "C" fn tray_status_icon_set_visible(
    tray: *mut TrayStatusIcon,
    visible: c_int,
) {
    if tray.is_null() {
        return;
    }
    
    unsafe {
        let tray = &*tray;
        tray.set_visible(visible != 0);
    }
}

#[no_mangle]
pub extern "C" fn tray_status_icon_is_embedded(tray: *mut TrayStatusIcon) -> c_int {
    if tray.is_null() {
        return 0;
    }
    
    unsafe {
        let tray = &*tray;
        if tray.is_embedded() { 1 } else { 0 }
    }
}