use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_void};
use std::sync::Mutex;
use gtk::prelude::*;
use gtk::{
    Dialog, Window, Notebook, Box as GtkBox, Orientation, 
    Frame, Grid, Label, SpinButton, CheckButton, Button, Align
};
use glib::signal::SignalHandlerId;
use glib::translate::FromGlibPtrNone;
use once_cell::sync::Lazy;

// Store callbacks in a global mutex since GTK callbacks can't capture arbitrary data
// Using usize instead of *mut c_void to make it Send+Sync
static CALLBACK_STORE: Lazy<Mutex<Option<(SettingsDialogCallback, usize)>>> = 
    Lazy::new(|| Mutex::new(None));

pub type SettingsDialogCallback = extern "C" fn(*const c_char, *mut c_void);

// External functions from config_rust.h - these will be provided by the main application
extern "C" {
    fn settings_new_default() -> *mut Settings;
    fn settings_free(settings: *mut Settings);
    fn settings_set_sound_type(settings: *mut Settings, sound_type: *const c_char);
    fn settings_set_work_start_sound(settings: *mut Settings, path: *const c_char);
    fn settings_set_break_start_sound(settings: *mut Settings, path: *const c_char);
}

// Settings structure (should match config_rust.h)
#[repr(C)]
pub struct Settings {
    pub work_duration: c_int,
    pub short_break_duration: c_int,
    pub long_break_duration: c_int,
    pub sessions_until_long_break: c_int,
    pub auto_start_work_after_break: bool,
    pub enable_idle_detection: bool,
    pub idle_timeout_minutes: c_int,
    pub enable_sounds: bool,
    pub sound_volume: f64,
    pub sound_type: *mut c_char,
    pub work_start_sound: *mut c_char,
    pub break_start_sound: *mut c_char,
    pub session_complete_sound: *mut c_char,
    pub timer_finish_sound: *mut c_char,
}

pub struct SettingsDialog {
    dialog: Dialog,
    notebook: Notebook,
    
    // Timer tab widgets
    work_duration_spin: SpinButton,
    short_break_spin: SpinButton,
    long_break_spin: SpinButton,
    sessions_spin: SpinButton,
    
    // Misc tab widgets
    auto_start_check: CheckButton,
    enable_sounds_check: CheckButton,
    enable_idle_detection_check: CheckButton,
    idle_timeout_spin: SpinButton,
    idle_timeout_box: GtkBox,
    
    // Dialog buttons
    restore_defaults_button: Button,
    cancel_button: Button,
    ok_button: Button,
    
    // Store signal handler IDs for cleanup
    signal_handlers: Vec<(glib::Object, SignalHandlerId)>,
}

impl SettingsDialog {
    fn new(parent: *mut gtk_sys::GtkWindow, settings: &Settings) -> Self {
        // Create dialog
        let dialog = Dialog::new();
        dialog.set_title("Settings");
        if !parent.is_null() {
            unsafe {
                let parent_window = Window::from_glib_none(parent);
                dialog.set_transient_for(Some(&parent_window));
            }
        }
        dialog.set_modal(true);
        dialog.set_default_size(400, 300);
        dialog.set_resizable(false);
        
        // Create main container
        let content_area = dialog.content_area();
        
        // Create notebook for tabs
        let notebook = Notebook::new();
        content_area.add(&notebook);
        
        // Create Timer tab
        let timer_tab = GtkBox::new(Orientation::Vertical, 10);
        timer_tab.set_border_width(20);
        
        let timer_label = Label::new(Some("Timer"));
        notebook.append_page(&timer_tab, Some(&timer_label));
        
        // Timer Durations section
        let durations_frame = Frame::new(Some("Timer Durations"));
        timer_tab.pack_start(&durations_frame, false, false, 0);
        
        let durations_grid = Grid::new();
        durations_grid.set_row_spacing(10);
        durations_grid.set_column_spacing(10);
        durations_grid.set_border_width(15);
        durations_frame.add(&durations_grid);
        
        // Work Duration
        let work_label = Label::new(Some("Work Duration:"));
        work_label.set_halign(Align::Start);
        durations_grid.attach(&work_label, 0, 0, 1, 1);
        
        let work_duration_spin = SpinButton::with_range(1.0, 120.0, 1.0);
        work_duration_spin.set_value(settings.work_duration as f64);
        durations_grid.attach(&work_duration_spin, 1, 0, 1, 1);
        
        let work_min_label = Label::new(Some("min"));
        durations_grid.attach(&work_min_label, 2, 0, 1, 1);
        
        // Short Break
        let short_break_label = Label::new(Some("Short Break:"));
        short_break_label.set_halign(Align::Start);
        durations_grid.attach(&short_break_label, 0, 1, 1, 1);
        
        let short_break_spin = SpinButton::with_range(1.0, 60.0, 1.0);
        short_break_spin.set_value(settings.short_break_duration as f64);
        durations_grid.attach(&short_break_spin, 1, 1, 1, 1);
        
        let short_min_label = Label::new(Some("min"));
        durations_grid.attach(&short_min_label, 2, 1, 1, 1);
        
        // Long Break
        let long_break_label = Label::new(Some("Long Break:"));
        long_break_label.set_halign(Align::Start);
        durations_grid.attach(&long_break_label, 0, 2, 1, 1);
        
        let long_break_spin = SpinButton::with_range(5.0, 120.0, 1.0);
        long_break_spin.set_value(settings.long_break_duration as f64);
        durations_grid.attach(&long_break_spin, 1, 2, 1, 1);
        
        let long_min_label = Label::new(Some("min"));
        durations_grid.attach(&long_min_label, 2, 2, 1, 1);
        
        // Sessions until long break
        let sessions_label = Label::new(Some("Sessions until Long Break:"));
        sessions_label.set_halign(Align::Start);
        durations_grid.attach(&sessions_label, 0, 3, 1, 1);
        
        let sessions_spin = SpinButton::with_range(2.0, 10.0, 1.0);
        sessions_spin.set_value(settings.sessions_until_long_break as f64);
        durations_grid.attach(&sessions_spin, 1, 3, 1, 1);
        
        // Create Misc tab
        let misc_tab = GtkBox::new(Orientation::Vertical, 10);
        misc_tab.set_border_width(20);
        
        let misc_label = Label::new(Some("Misc"));
        notebook.append_page(&misc_tab, Some(&misc_label));
        
        // Behavior Settings
        let behavior_frame = Frame::new(Some("Behavior Settings"));
        misc_tab.pack_start(&behavior_frame, false, false, 0);
        
        let behavior_box = GtkBox::new(Orientation::Vertical, 10);
        behavior_box.set_border_width(15);
        behavior_frame.add(&behavior_box);
        
        let auto_start_check = CheckButton::with_label("Auto-start work after breaks end");
        auto_start_check.set_active(settings.auto_start_work_after_break);
        auto_start_check.set_margin_bottom(8);
        behavior_box.pack_start(&auto_start_check, false, false, 0);
        
        let enable_sounds_check = CheckButton::with_label("Enable sound alerts");
        enable_sounds_check.set_active(settings.enable_sounds);
        behavior_box.pack_start(&enable_sounds_check, false, false, 0);
        
        // Idle detection section
        enable_sounds_check.set_margin_top(8);
        let enable_idle_detection_check = CheckButton::with_label("Auto-pause when idle");
        enable_idle_detection_check.set_active(settings.enable_idle_detection);
        enable_idle_detection_check.set_margin_top(8);
        behavior_box.pack_start(&enable_idle_detection_check, false, false, 0);
        
        // Idle timeout configuration
        let idle_timeout_box = GtkBox::new(Orientation::Horizontal, 10);
        idle_timeout_box.set_margin_start(24);
        idle_timeout_box.set_margin_top(5);
        behavior_box.pack_start(&idle_timeout_box, false, false, 0);
        
        let idle_timeout_label = Label::new(Some("Idle timeout:"));
        idle_timeout_box.pack_start(&idle_timeout_label, false, false, 0);
        
        let idle_timeout_spin = SpinButton::with_range(1.0, 30.0, 1.0);
        idle_timeout_spin.set_value(settings.idle_timeout_minutes as f64);
        idle_timeout_box.pack_start(&idle_timeout_spin, false, false, 0);
        
        let idle_min_label = Label::new(Some("minutes"));
        idle_timeout_box.pack_start(&idle_min_label, false, false, 0);
        
        // Set sensitivity based on checkbox state
        idle_timeout_box.set_sensitive(settings.enable_idle_detection);
        
        // Dialog buttons
        let restore_defaults_button = Button::with_label("Restore Defaults");
        dialog.add_action_widget(&restore_defaults_button, gtk::ResponseType::Other(100).into());
        
        let cancel_button = Button::with_label("Cancel");
        dialog.add_action_widget(&cancel_button, gtk::ResponseType::Cancel.into());
        
        let ok_button = Button::with_label("OK");
        ok_button.style_context().add_class("suggested-action");
        dialog.add_action_widget(&ok_button, gtk::ResponseType::Ok.into());
        
        let mut signal_handlers = Vec::new();
        
        // Connect toggle signal to enable/disable timeout spinner
        let idle_box_clone = idle_timeout_box.clone();
        let idle_handler = enable_idle_detection_check.connect_toggled(move |button| {
            let active = button.is_active();
            idle_box_clone.set_sensitive(active);
        });
        signal_handlers.push((enable_idle_detection_check.clone().upcast(), idle_handler));
        
        // Connect button signals
        let restore_handler = restore_defaults_button.connect_clicked(|_| {
            trigger_callback("restore_defaults");
        });
        signal_handlers.push((restore_defaults_button.clone().upcast(), restore_handler));
        
        let cancel_handler = cancel_button.connect_clicked(|_| {
            trigger_callback("cancel");
        });
        signal_handlers.push((cancel_button.clone().upcast(), cancel_handler));
        
        let ok_handler = ok_button.connect_clicked(|_| {
            trigger_callback("ok");
        });
        signal_handlers.push((ok_button.clone().upcast(), ok_handler));
        
        SettingsDialog {
            dialog,
            notebook,
            work_duration_spin,
            short_break_spin,
            long_break_spin,
            sessions_spin,
            auto_start_check,
            enable_sounds_check,
            enable_idle_detection_check,
            idle_timeout_spin,
            idle_timeout_box,
            restore_defaults_button,
            cancel_button,
            ok_button,
            signal_handlers,
        }
    }
    
    fn restore_defaults(&self) {
        unsafe {
            let defaults = settings_new_default();
            if !defaults.is_null() {
                let defaults_ref = &*defaults;
                
                // Reset all controls to defaults
                self.work_duration_spin.set_value(defaults_ref.work_duration as f64);
                self.short_break_spin.set_value(defaults_ref.short_break_duration as f64);
                self.long_break_spin.set_value(defaults_ref.long_break_duration as f64);
                self.sessions_spin.set_value(defaults_ref.sessions_until_long_break as f64);
                self.auto_start_check.set_active(defaults_ref.auto_start_work_after_break);
                self.enable_sounds_check.set_active(defaults_ref.enable_sounds);
                self.enable_idle_detection_check.set_active(defaults_ref.enable_idle_detection);
                self.idle_timeout_spin.set_value(defaults_ref.idle_timeout_minutes as f64);
                
                settings_free(defaults);
            }
        }
    }
    
    fn get_settings(&self) -> Box<Settings> {
        let settings = Box::new(Settings {
            work_duration: self.work_duration_spin.value_as_int(),
            short_break_duration: self.short_break_spin.value_as_int(),
            long_break_duration: self.long_break_spin.value_as_int(),
            sessions_until_long_break: self.sessions_spin.value_as_int(),
            auto_start_work_after_break: self.auto_start_check.is_active(),
            enable_idle_detection: self.enable_idle_detection_check.is_active(),
            idle_timeout_minutes: self.idle_timeout_spin.value_as_int(),
            enable_sounds: self.enable_sounds_check.is_active(),
            sound_volume: 0.7, // Fixed reasonable volume
            sound_type: std::ptr::null_mut(),
            work_start_sound: std::ptr::null_mut(),
            break_start_sound: std::ptr::null_mut(),
            session_complete_sound: std::ptr::null_mut(),
            timer_finish_sound: std::ptr::null_mut(),
        });
        
        // Set sound type to "chimes"
        unsafe {
            let sound_type = CString::new("chimes").unwrap();
            settings_set_sound_type(&*settings as *const Settings as *mut Settings, sound_type.as_ptr());
            settings_set_work_start_sound(&*settings as *const Settings as *mut Settings, std::ptr::null());
            settings_set_break_start_sound(&*settings as *const Settings as *mut Settings, std::ptr::null());
        }
        
        settings
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
pub extern "C" fn settings_dialog_new(
    parent: *mut gtk_sys::GtkWindow, 
    settings: *const Settings,
    _audio: *mut c_void, // AudioManager not used in simplified version
) -> *mut SettingsDialog {
    gtk::init().expect("Failed to initialize GTK");
    
    if settings.is_null() {
        return std::ptr::null_mut();
    }
    
    unsafe {
        let settings_ref = &*settings;
        Box::into_raw(Box::new(SettingsDialog::new(parent, settings_ref)))
    }
}

#[no_mangle]
pub extern "C" fn settings_dialog_free(dialog: *mut SettingsDialog) {
    if dialog.is_null() {
        return;
    }
    
    unsafe {
        let mut dialog = Box::from_raw(dialog);
        
        // Disconnect all signal handlers
        for (obj, handler_id) in dialog.signal_handlers.drain(..) {
            obj.disconnect(handler_id);
        }
        
        dialog.dialog.destroy();
        
        drop(dialog);
    }
}

#[no_mangle]
pub extern "C" fn settings_dialog_set_callback(
    dialog: *mut SettingsDialog,
    callback: Option<SettingsDialogCallback>,
    user_data: *mut c_void,
) {
    if dialog.is_null() {
        return;
    }
    
    if let Ok(mut guard) = CALLBACK_STORE.lock() {
        if let Some(callback) = callback {
            *guard = Some((callback, user_data as usize));
        } else {
            *guard = None;
        }
    }
}

#[no_mangle]
pub extern "C" fn settings_dialog_show(dialog: *mut SettingsDialog) {
    if dialog.is_null() {
        return;
    }
    
    unsafe {
        let dialog = &*dialog;
        dialog.dialog.show_all();
    }
}

#[no_mangle]
pub extern "C" fn settings_dialog_get_settings(dialog: *mut SettingsDialog) -> *mut Settings {
    if dialog.is_null() {
        return std::ptr::null_mut();
    }
    
    unsafe {
        let dialog = &*dialog;
        Box::into_raw(dialog.get_settings())
    }
}

// Handle restore defaults callback
#[no_mangle]
pub extern "C" fn settings_dialog_restore_defaults(dialog: *mut SettingsDialog) {
    if dialog.is_null() {
        return;
    }
    
    unsafe {
        let dialog = &*dialog;
        dialog.restore_defaults();
    }
}