use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_void};
use std::sync::Mutex;
use gtk::prelude::*;
use gtk::{Window, WindowType, Box as GtkBox, Label, Button, Orientation, Align, CssProvider};
use glib::Propagation;
use gdk::Display;
use glib::signal::SignalHandlerId;
use once_cell::sync::Lazy;

// Store callbacks in a global mutex since GTK callbacks can't capture arbitrary data
// Using usize instead of *mut c_void to make it Send+Sync
static CALLBACK_STORE: Lazy<Mutex<Option<(BreakOverlayCallback, usize)>>> = 
    Lazy::new(|| Mutex::new(None));

pub type BreakOverlayCallback = extern "C" fn(*const c_char, *mut c_void);

pub struct BreakOverlay {
    window: Window,
    main_box: GtkBox,
    title_label: Label,
    time_label: Label,
    message_label: Label,
    button_box: GtkBox,
    skip_button: Button,
    extend_button: Button,
    pause_button: Button,
    dismiss_label: Label,
    secondary_windows: Vec<Window>,
    // Store signal handler IDs for cleanup
    signal_handlers: Vec<(glib::Object, SignalHandlerId)>,
}

impl BreakOverlay {
    fn new() -> Self {
        // Create fullscreen window
        let window = Window::new(WindowType::Toplevel);
        window.set_title("Break Time");
        window.set_decorated(false);
        window.set_skip_taskbar_hint(true);
        window.set_skip_pager_hint(true);
        window.set_keep_above(true);
        window.stick(); // Show on all workspaces
        window.fullscreen();
        
        // Set black background
        window.style_context().add_class("break-overlay");
        
        // Create main container
        let main_box = GtkBox::new(Orientation::Vertical, 40);
        main_box.set_halign(Align::Center);
        main_box.set_valign(Align::Center);
        window.add(&main_box);
        
        // Break type title
        let title_label = Label::new(Some("Short Break"));
        title_label.style_context().add_class("break-title");
        title_label.set_halign(Align::Center);
        main_box.pack_start(&title_label, false, false, 0);
        
        // Countdown timer
        let time_label = Label::new(Some("04:04"));
        time_label.style_context().add_class("break-timer");
        time_label.set_halign(Align::Center);
        main_box.pack_start(&time_label, false, false, 0);
        
        // Motivational message
        let message_label = Label::new(Some("Take a quick breather"));
        message_label.style_context().add_class("break-message");
        message_label.set_halign(Align::Center);
        main_box.pack_start(&message_label, false, false, 0);
        
        // Button container
        let button_box = GtkBox::new(Orientation::Horizontal, 20);
        button_box.set_halign(Align::Center);
        main_box.pack_start(&button_box, false, false, 0);
        
        // Skip Break button
        let skip_button = Button::with_label("Skip Break (S)");
        skip_button.style_context().add_class("break-button");
        skip_button.style_context().add_class("break-button-destructive");
        skip_button.set_size_request(140, 40);
        button_box.pack_start(&skip_button, false, false, 0);
        
        // Extend Break button
        let extend_button = Button::with_label("Extend Break (E)");
        extend_button.style_context().add_class("break-button");
        extend_button.style_context().add_class("break-button-normal");
        extend_button.set_size_request(140, 40);
        button_box.pack_start(&extend_button, false, false, 0);
        
        // Pause button
        let pause_button = Button::with_label("Pause (P)");
        pause_button.style_context().add_class("break-button");
        pause_button.style_context().add_class("break-button-warning");
        pause_button.set_size_request(140, 40);
        button_box.pack_start(&pause_button, false, false, 0);
        
        // ESC to dismiss label
        let dismiss_label = Label::new(Some("Press ESC to dismiss"));
        dismiss_label.style_context().add_class("break-dismiss");
        dismiss_label.set_halign(Align::Center);
        dismiss_label.set_margin_top(40);
        main_box.pack_start(&dismiss_label, false, false, 0);
        
        let mut signal_handlers = Vec::new();
        
        // Connect button signals
        let skip_handler = skip_button.connect_clicked(|_| {
            trigger_callback("skip_break");
        });
        signal_handlers.push((skip_button.clone().upcast(), skip_handler));
        
        let extend_handler = extend_button.connect_clicked(|_| {
            trigger_callback("extend_break");
        });
        signal_handlers.push((extend_button.clone().upcast(), extend_handler));
        
        let pause_handler = pause_button.connect_clicked(|_| {
            trigger_callback("pause");
        });
        signal_handlers.push((pause_button.clone().upcast(), pause_handler));
        
        // Connect key press event
        let key_handler = window.connect_key_press_event(|_, event| {
            match event.keyval() {
                gdk::keys::constants::Escape => {
                    trigger_callback("dismiss");
                    Propagation::Stop
                },
                gdk::keys::constants::s | gdk::keys::constants::S => {
                    trigger_callback("skip_break");
                    Propagation::Stop
                },
                gdk::keys::constants::e | gdk::keys::constants::E => {
                    trigger_callback("extend_break");
                    Propagation::Stop
                },
                gdk::keys::constants::p | gdk::keys::constants::P => {
                    trigger_callback("pause");
                    Propagation::Stop
                },
                gdk::keys::constants::r | gdk::keys::constants::R => {
                    trigger_callback("pause");
                    Propagation::Stop
                },
                _ => Propagation::Proceed,
            }
        });
        signal_handlers.push((window.clone().upcast(), key_handler));
        
        // Load CSS
        load_css();
        
        // Initially hidden
        window.hide();
        
        BreakOverlay {
            window,
            main_box,
            title_label,
            time_label,
            message_label,
            button_box,
            skip_button,
            extend_button,
            pause_button,
            dismiss_label,
            secondary_windows: Vec::new(),
            signal_handlers,
        }
    }
    
    fn create_secondary_overlays(&mut self) {
        // Clean up existing secondary windows
        self.destroy_secondary_overlays();
        
        let display = Display::default().expect("Could not get default display");
        let n_monitors = display.n_monitors();
        
        // Get primary monitor
        let primary_monitor = display.primary_monitor();
        
        // Create overlay windows for all other monitors
        for i in 0..n_monitors {
            let monitor = display.monitor(i).expect("Could not get monitor");
            
            // Skip the primary monitor
            if primary_monitor.as_ref() == Some(&monitor) {
                continue;
            }
            
            let geometry = monitor.geometry();
            
            // Create a simple overlay window
            let secondary_window = Window::new(WindowType::Toplevel);
            secondary_window.set_decorated(false);
            secondary_window.set_skip_taskbar_hint(true);
            secondary_window.set_skip_pager_hint(true);
            secondary_window.set_keep_above(true);
            secondary_window.stick();
            
            // Position and size the window
            secondary_window.move_(geometry.x(), geometry.y());
            secondary_window.set_default_size(geometry.width(), geometry.height());
            secondary_window.fullscreen();
            
            // Set black background
            secondary_window.style_context().add_class("break-overlay");
            
            // Connect key press event
            let key_handler = secondary_window.connect_key_press_event(|_, event| {
                if event.keyval() == gdk::keys::constants::Escape {
                    trigger_callback("dismiss");
                    Propagation::Stop
                } else {
                    Propagation::Proceed
                }
            });
            self.signal_handlers.push((secondary_window.clone().upcast(), key_handler));
            
            self.secondary_windows.push(secondary_window);
        }
    }
    
    fn destroy_secondary_overlays(&mut self) {
        for window in &self.secondary_windows {
            unsafe {
                window.destroy();
            }
        }
        self.secondary_windows.clear();
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

fn load_css() {
    let css_provider = CssProvider::new();
    let css_data = r#"
        .break-overlay { background-color: #000000; }
        .break-title { font-size: 48px; color: #4dd0e1; font-weight: bold; }
        .break-timer { font-size: 96px; color: #ffffff; font-weight: bold; font-family: monospace; }
        .break-message { font-size: 24px; color: #888888; }
        .break-button { 
          min-width: 120px; min-height: 40px; 
          font-size: 16px; font-weight: bold; 
          border-radius: 8px; 
        }
        .break-button-destructive { 
          background-color: #d32f2f; color: #ffffff; 
          border: 2px solid #b71c1c; 
        }
        .break-button-destructive:hover { background-color: #f44336; }
        .break-button-normal { 
          background-color: #1976d2; color: #ffffff; 
          border: 2px solid #0d47a1; 
        }
        .break-button-normal:hover { background-color: #2196f3; }
        .break-button-warning { 
          background-color: #f57c00; color: #ffffff; 
          border: 2px solid #e65100; 
        }
        .break-button-warning:hover { background-color: #ff9800; }
        .break-dismiss { font-size: 16px; color: #666666; }
    "#;
    
    css_provider.load_from_data(css_data.as_bytes()).unwrap();
    
    gtk::StyleContext::add_provider_for_screen(
        &gdk::Screen::default().expect("Could not get default screen"),
        &css_provider,
        gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
    );
}

fn get_motivational_message(break_type: &str) -> &'static str {
    match break_type {
        "Short Break" => "Take a quick breather",
        "Long Break" => "Time for a longer rest",
        "Paused" => "Break timer paused",
        _ => "Take a moment to relax",
    }
}

// C FFI exports
#[no_mangle]
pub extern "C" fn break_overlay_new() -> *mut BreakOverlay {
    gtk::init().expect("Failed to initialize GTK");
    Box::into_raw(Box::new(BreakOverlay::new()))
}

#[no_mangle]
pub extern "C" fn break_overlay_free(overlay: *mut BreakOverlay) {
    if overlay.is_null() {
        return;
    }
    
    unsafe {
        let mut overlay = Box::from_raw(overlay);
        
        // Disconnect all signal handlers
        for (obj, handler_id) in overlay.signal_handlers.drain(..) {
            obj.disconnect(handler_id);
        }
        
        overlay.destroy_secondary_overlays();
        unsafe {
            overlay.window.destroy();
        }
        
        drop(overlay);
    }
}

#[no_mangle]
pub extern "C" fn break_overlay_set_callback(
    _overlay: *mut BreakOverlay,
    callback: Option<BreakOverlayCallback>,
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
pub extern "C" fn break_overlay_show(
    overlay: *mut BreakOverlay,
    break_type: *const c_char,
    minutes: c_int,
    seconds: c_int,
) {
    if overlay.is_null() || break_type.is_null() {
        return;
    }
    
    unsafe {
        let overlay = &mut *overlay;
        let break_type_str = CStr::from_ptr(break_type).to_str().unwrap_or("Break");
        
        // Update content
        overlay.title_label.set_text(break_type_str);
        overlay.message_label.set_text(get_motivational_message(break_type_str));
        
        let time_text = format!("{:02}:{:02}", minutes, seconds);
        overlay.time_label.set_text(&time_text);
        
        // Create secondary overlays
        overlay.create_secondary_overlays();
        
        // Show main window
        overlay.window.show_all();
        overlay.window.present();
        
        // Show secondary windows
        for window in &overlay.secondary_windows {
            window.show_all();
            window.present();
        }
        
        // Grab focus
        overlay.window.grab_focus();
    }
}

#[no_mangle]
pub extern "C" fn break_overlay_hide(overlay: *mut BreakOverlay) {
    if overlay.is_null() {
        return;
    }
    
    unsafe {
        let overlay = &mut *overlay;
        overlay.window.hide();
        overlay.destroy_secondary_overlays();
    }
}

#[no_mangle]
pub extern "C" fn break_overlay_update_time(
    overlay: *mut BreakOverlay,
    minutes: c_int,
    seconds: c_int,
) {
    if overlay.is_null() {
        return;
    }
    
    unsafe {
        let overlay = &*overlay;
        let time_text = format!("{:02}:{:02}", minutes, seconds);
        overlay.time_label.set_text(&time_text);
    }
}

#[no_mangle]
pub extern "C" fn break_overlay_update_type(
    overlay: *mut BreakOverlay,
    break_type: *const c_char,
) {
    if overlay.is_null() || break_type.is_null() {
        return;
    }
    
    unsafe {
        let overlay = &*overlay;
        let break_type_str = CStr::from_ptr(break_type).to_str().unwrap_or("Break");
        
        overlay.title_label.set_text(break_type_str);
        overlay.message_label.set_text(get_motivational_message(break_type_str));
        
        // Update title color based on break type
        let context = overlay.title_label.style_context();
        context.remove_class("break-title-short");
        context.remove_class("break-title-long");
        context.remove_class("break-title-paused");
        
        match break_type_str {
            "Short Break" => context.add_class("break-title-short"),
            "Long Break" => context.add_class("break-title-long"),
            "Paused" => context.add_class("break-title-paused"),
            _ => {}
        }
    }
}

#[no_mangle]
pub extern "C" fn break_overlay_is_visible(overlay: *mut BreakOverlay) -> c_int {
    if overlay.is_null() {
        return 0;
    }
    
    unsafe {
        let overlay = &*overlay;
        if overlay.window.is_visible() { 1 } else { 0 }
    }
}

#[no_mangle]
pub extern "C" fn break_overlay_update_pause_button(
    overlay: *mut BreakOverlay,
    label: *const c_char,
) {
    if overlay.is_null() || label.is_null() {
        return;
    }
    
    unsafe {
        let overlay = &*overlay;
        let label_str = CStr::from_ptr(label).to_str().unwrap_or("Pause");
        
        let label_with_shortcut = match label_str {
            "Pause" => "Pause (P)",
            "Resume" => "Resume (R)",
            _ => label_str,
        };
        
        overlay.pause_button.set_label(label_with_shortcut);
    }
}
