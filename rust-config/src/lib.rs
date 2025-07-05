use std::ffi::{CStr, CString};
use std::fs;
use std::path::PathBuf;
use libc::{c_char, c_int};
use serde::{Deserialize, Serialize};

#[repr(C)]
pub struct Config {
    config_dir: Option<PathBuf>,
    config_file: Option<PathBuf>,
    use_persistent: bool,
}

// Internal Rust settings structure for serialization
#[derive(Debug, Clone, Serialize, Deserialize)]
struct SettingsInternal {
    work_duration: i32,
    short_break_duration: i32,
    long_break_duration: i32,
    sessions_until_long_break: i32,
    auto_start_work_after_break: bool,
    #[serde(default)]
    enable_idle_detection: bool,
    #[serde(default = "default_idle_timeout")]
    idle_timeout_minutes: i32,
    enable_sounds: bool,
    sound_volume: f64,
    sound_type: Option<String>,
    work_start_sound: Option<String>,
    break_start_sound: Option<String>,
    session_complete_sound: Option<String>,
    timer_finish_sound: Option<String>,
}

fn default_idle_timeout() -> i32 {
    2
}

// C-compatible settings structure
#[repr(C)]
pub struct Settings {
    // Timer settings
    pub work_duration: i32,              // minutes (1-120)
    pub short_break_duration: i32,       // minutes (1-60)
    pub long_break_duration: i32,        // minutes (5-120)
    pub sessions_until_long_break: i32,  // count (2-10)
    
    // Behavior settings
    pub auto_start_work_after_break: c_int,  // gboolean
    pub enable_idle_detection: c_int,        // gboolean
    pub idle_timeout_minutes: i32,           // minutes (1-30)
    
    // Audio settings
    pub enable_sounds: c_int,                // gboolean
    pub sound_volume: f64,                   // 0.0-1.0
    pub sound_type: *mut c_char,             // "chimes" or "custom"
    pub work_start_sound: *mut c_char,       // file path or NULL
    pub break_start_sound: *mut c_char,      // file path or NULL
    pub session_complete_sound: *mut c_char, // file path or NULL
    pub timer_finish_sound: *mut c_char,     // file path or NULL
}

impl Settings {
    fn to_internal(&self) -> SettingsInternal {
        SettingsInternal {
            work_duration: self.work_duration,
            short_break_duration: self.short_break_duration,
            long_break_duration: self.long_break_duration,
            sessions_until_long_break: self.sessions_until_long_break,
            auto_start_work_after_break: self.auto_start_work_after_break != 0,
            enable_idle_detection: self.enable_idle_detection != 0,
            idle_timeout_minutes: self.idle_timeout_minutes,
            enable_sounds: self.enable_sounds != 0,
            sound_volume: self.sound_volume,
            sound_type: unsafe {
                if self.sound_type.is_null() {
                    None
                } else {
                    CStr::from_ptr(self.sound_type).to_str().ok().map(|s| s.to_string())
                }
            },
            work_start_sound: unsafe {
                if self.work_start_sound.is_null() {
                    None
                } else {
                    CStr::from_ptr(self.work_start_sound).to_str().ok().map(|s| s.to_string())
                }
            },
            break_start_sound: unsafe {
                if self.break_start_sound.is_null() {
                    None
                } else {
                    CStr::from_ptr(self.break_start_sound).to_str().ok().map(|s| s.to_string())
                }
            },
            session_complete_sound: unsafe {
                if self.session_complete_sound.is_null() {
                    None
                } else {
                    CStr::from_ptr(self.session_complete_sound).to_str().ok().map(|s| s.to_string())
                }
            },
            timer_finish_sound: unsafe {
                if self.timer_finish_sound.is_null() {
                    None
                } else {
                    CStr::from_ptr(self.timer_finish_sound).to_str().ok().map(|s| s.to_string())
                }
            },
        }
    }
    
    fn from_internal(internal: SettingsInternal) -> Self {
        Settings {
            work_duration: internal.work_duration,
            short_break_duration: internal.short_break_duration,
            long_break_duration: internal.long_break_duration,
            sessions_until_long_break: internal.sessions_until_long_break,
            auto_start_work_after_break: if internal.auto_start_work_after_break { 1 } else { 0 },
            enable_idle_detection: if internal.enable_idle_detection { 1 } else { 0 },
            idle_timeout_minutes: internal.idle_timeout_minutes,
            enable_sounds: if internal.enable_sounds { 1 } else { 0 },
            sound_volume: internal.sound_volume,
            sound_type: internal.sound_type
                .and_then(|s| CString::new(s).ok())
                .map(|s| s.into_raw())
                .unwrap_or(std::ptr::null_mut()),
            work_start_sound: internal.work_start_sound
                .and_then(|s| CString::new(s).ok())
                .map(|s| s.into_raw())
                .unwrap_or(std::ptr::null_mut()),
            break_start_sound: internal.break_start_sound
                .and_then(|s| CString::new(s).ok())
                .map(|s| s.into_raw())
                .unwrap_or(std::ptr::null_mut()),
            session_complete_sound: internal.session_complete_sound
                .and_then(|s| CString::new(s).ok())
                .map(|s| s.into_raw())
                .unwrap_or(std::ptr::null_mut()),
            timer_finish_sound: internal.timer_finish_sound
                .and_then(|s| CString::new(s).ok())
                .map(|s| s.into_raw())
                .unwrap_or(std::ptr::null_mut()),
        }
    }
}

impl Clone for Settings {
    fn clone(&self) -> Self {
        Settings {
            work_duration: self.work_duration,
            short_break_duration: self.short_break_duration,
            long_break_duration: self.long_break_duration,
            sessions_until_long_break: self.sessions_until_long_break,
            auto_start_work_after_break: self.auto_start_work_after_break,
            enable_idle_detection: self.enable_idle_detection,
            idle_timeout_minutes: self.idle_timeout_minutes,
            enable_sounds: self.enable_sounds,
            sound_volume: self.sound_volume,
            sound_type: if self.sound_type.is_null() {
                std::ptr::null_mut()
            } else {
                unsafe {
                    CStr::from_ptr(self.sound_type)
                        .to_str()
                        .ok()
                        .and_then(|s| CString::new(s).ok())
                        .map(|s| s.into_raw())
                        .unwrap_or(std::ptr::null_mut())
                }
            },
            work_start_sound: if self.work_start_sound.is_null() {
                std::ptr::null_mut()
            } else {
                unsafe {
                    CStr::from_ptr(self.work_start_sound)
                        .to_str()
                        .ok()
                        .and_then(|s| CString::new(s).ok())
                        .map(|s| s.into_raw())
                        .unwrap_or(std::ptr::null_mut())
                }
            },
            break_start_sound: if self.break_start_sound.is_null() {
                std::ptr::null_mut()
            } else {
                unsafe {
                    CStr::from_ptr(self.break_start_sound)
                        .to_str()
                        .ok()
                        .and_then(|s| CString::new(s).ok())
                        .map(|s| s.into_raw())
                        .unwrap_or(std::ptr::null_mut())
                }
            },
            session_complete_sound: if self.session_complete_sound.is_null() {
                std::ptr::null_mut()
            } else {
                unsafe {
                    CStr::from_ptr(self.session_complete_sound)
                        .to_str()
                        .ok()
                        .and_then(|s| CString::new(s).ok())
                        .map(|s| s.into_raw())
                        .unwrap_or(std::ptr::null_mut())
                }
            },
            timer_finish_sound: if self.timer_finish_sound.is_null() {
                std::ptr::null_mut()
            } else {
                unsafe {
                    CStr::from_ptr(self.timer_finish_sound)
                        .to_str()
                        .ok()
                        .and_then(|s| CString::new(s).ok())
                        .map(|s| s.into_raw())
                        .unwrap_or(std::ptr::null_mut())
                }
            },
        }
    }
}

impl Drop for Settings {
    fn drop(&mut self) {
        // Free all C strings
        if !self.sound_type.is_null() {
            unsafe { drop(CString::from_raw(self.sound_type)); }
        }
        if !self.work_start_sound.is_null() {
            unsafe { drop(CString::from_raw(self.work_start_sound)); }
        }
        if !self.break_start_sound.is_null() {
            unsafe { drop(CString::from_raw(self.break_start_sound)); }
        }
        if !self.session_complete_sound.is_null() {
            unsafe { drop(CString::from_raw(self.session_complete_sound)); }
        }
        if !self.timer_finish_sound.is_null() {
            unsafe { drop(CString::from_raw(self.timer_finish_sound)); }
        }
    }
}

impl Default for Settings {
    fn default() -> Self {
        let internal = SettingsInternal {
            work_duration: 25,
            short_break_duration: 5,
            long_break_duration: 15,
            sessions_until_long_break: 4,
            auto_start_work_after_break: true,
            enable_idle_detection: false,
            idle_timeout_minutes: 2,
            enable_sounds: true,
            sound_volume: 0.7,
            sound_type: Some("chimes".to_string()),
            work_start_sound: None,
            break_start_sound: None,
            session_complete_sound: None,
            timer_finish_sound: None,
        };
        
        Settings::from_internal(internal)
    }
}

// C FFI functions
#[no_mangle]
pub extern "C" fn config_new(use_persistent: c_int) -> *mut Config {
    let use_persistent = use_persistent != 0;
    
    let (config_dir, config_file) = if use_persistent {
        // Create config directory path: ~/.config/commodoro/
        if let Some(home) = std::env::var_os("HOME") {
            let config_base = PathBuf::from(home).join(".config");
            let config_dir = config_base.join("commodoro");
            let config_file = config_dir.join("config.json");
            (Some(config_dir), Some(config_file))
        } else {
            eprintln!("Failed to get HOME directory");
            (None, None)
        }
    } else {
        // In-memory config for test mode
        (None, None)
    };
    
    let config = Box::new(Config {
        config_dir,
        config_file,
        use_persistent,
    });
    
    Box::into_raw(config)
}

#[no_mangle]
pub extern "C" fn config_free(config: *mut Config) {
    if config.is_null() {
        return;
    }
    
    unsafe {
        drop(Box::from_raw(config));
    }
}

#[no_mangle]
pub extern "C" fn config_load_settings(config: *mut Config) -> *mut Settings {
    if config.is_null() {
        return settings_new_default();
    }
    
    unsafe {
        let config = &*config;
        
        // In-memory config always returns defaults
        if !config.use_persistent {
            println!("Using in-memory config (test mode)");
            return settings_new_default();
        }
        
        // Ensure config directory exists
        if let Some(ref config_dir) = config.config_dir {
            if let Err(e) = fs::create_dir_all(config_dir) {
                eprintln!("Failed to create config directory: {}", e);
                return settings_new_default();
            }
        }
        
        // Check if config file exists
        if let Some(ref config_file) = config.config_file {
            if !config_file.exists() {
                println!("Config file not found, using defaults: {}", config_file.display());
                return settings_new_default();
            }
            
            // Read and parse config file
            match fs::read_to_string(config_file) {
                Ok(mut contents) => {
                    // Fix locale-specific decimal separator (comma to period)
                    // This handles legacy config files that used comma as decimal separator
                    contents = contents.replace("\": 0,", "\": 0.");
                    
                    match serde_json::from_str::<SettingsInternal>(&contents) {
                        Ok(internal) => {
                            println!("Loaded settings from: {}", config_file.display());
                            return Box::into_raw(Box::new(Settings::from_internal(internal)));
                        }
                        Err(e) => {
                            eprintln!("Failed to parse config file: {}", e);
                            return settings_new_default();
                        }
                    }
                }
                Err(e) => {
                    eprintln!("Failed to read config file: {}", e);
                    return settings_new_default();
                }
            }
        }
        
        settings_new_default()
    }
}

#[no_mangle]
pub extern "C" fn config_save_settings(config: *mut Config, settings: *const Settings) -> c_int {
    if config.is_null() || settings.is_null() {
        return 0;
    }
    
    unsafe {
        let config = &*config;
        let settings = &*settings;
        
        // In-memory config doesn't save to disk
        if !config.use_persistent {
            println!("In-memory config: settings not saved to disk");
            return 1;  // Success, but no-op
        }
        
        // Ensure config directory exists
        if let Some(ref config_dir) = config.config_dir {
            if let Err(e) = fs::create_dir_all(config_dir) {
                eprintln!("Failed to create config directory: {}", e);
                return 0;
            }
        }
        
        // Write config file
        if let Some(ref config_file) = config.config_file {
            let internal = settings.to_internal();
            let json = match serde_json::to_string_pretty(&internal) {
                Ok(json) => json,
                Err(e) => {
                    eprintln!("Failed to serialize settings: {}", e);
                    return 0;
                }
            };
            
            match fs::write(config_file, json) {
                Ok(_) => {
                    println!("Saved settings to: {}", config_file.display());
                    return 1;
                }
                Err(e) => {
                    eprintln!("Failed to write config file: {}", e);
                    return 0;
                }
            }
        }
        
        0
    }
}

#[no_mangle]
pub extern "C" fn config_get_config_dir(config: *mut Config) -> *const c_char {
    if config.is_null() {
        return std::ptr::null();
    }
    
    unsafe {
        let config = &*config;
        
        if let Some(ref config_dir) = config.config_dir {
            if let Ok(path_str) = CString::new(config_dir.to_string_lossy().as_ref()) {
                // Leak the CString to keep it alive for C code
                return path_str.into_raw();
            }
        }
        
        std::ptr::null()
    }
}

// Settings functions
#[no_mangle]
pub extern "C" fn settings_new_default() -> *mut Settings {
    Box::into_raw(Box::new(Settings::default()))
}

#[no_mangle]
pub extern "C" fn settings_free(settings: *mut Settings) {
    if settings.is_null() {
        return;
    }
    
    unsafe {
        drop(Box::from_raw(settings));
    }
}

#[no_mangle]
pub extern "C" fn settings_copy(settings: *const Settings) -> *mut Settings {
    if settings.is_null() {
        return std::ptr::null_mut();
    }
    
    unsafe {
        let settings = &*settings;
        Box::into_raw(Box::new(settings.clone()))
    }
}

// Getter functions for C compatibility
#[no_mangle]
pub extern "C" fn settings_get_work_duration(settings: *const Settings) -> c_int {
    if settings.is_null() {
        return 25;
    }
    unsafe { (*settings).work_duration }
}

#[no_mangle]
pub extern "C" fn settings_get_short_break_duration(settings: *const Settings) -> c_int {
    if settings.is_null() {
        return 5;
    }
    unsafe { (*settings).short_break_duration }
}

#[no_mangle]
pub extern "C" fn settings_get_long_break_duration(settings: *const Settings) -> c_int {
    if settings.is_null() {
        return 15;
    }
    unsafe { (*settings).long_break_duration }
}

#[no_mangle]
pub extern "C" fn settings_get_sessions_until_long_break(settings: *const Settings) -> c_int {
    if settings.is_null() {
        return 4;
    }
    unsafe { (*settings).sessions_until_long_break }
}

#[no_mangle]
pub extern "C" fn settings_get_auto_start_work_after_break(settings: *const Settings) -> c_int {
    if settings.is_null() {
        return 1;
    }
    unsafe { (*settings).auto_start_work_after_break }
}

#[no_mangle]
pub extern "C" fn settings_get_enable_idle_detection(settings: *const Settings) -> c_int {
    if settings.is_null() {
        return 0;
    }
    unsafe { (*settings).enable_idle_detection }
}

#[no_mangle]
pub extern "C" fn settings_get_idle_timeout_minutes(settings: *const Settings) -> c_int {
    if settings.is_null() {
        return 2;
    }
    unsafe { (*settings).idle_timeout_minutes }
}

#[no_mangle]
pub extern "C" fn settings_get_enable_sounds(settings: *const Settings) -> c_int {
    if settings.is_null() {
        return 1;
    }
    unsafe { (*settings).enable_sounds }
}

#[no_mangle]
pub extern "C" fn settings_get_sound_volume(settings: *const Settings) -> f64 {
    if settings.is_null() {
        return 0.7;
    }
    unsafe { (*settings).sound_volume }
}

#[no_mangle]
pub extern "C" fn settings_get_sound_type(settings: *const Settings) -> *const c_char {
    if settings.is_null() {
        return std::ptr::null();
    }
    
    unsafe {
        let settings = &*settings;
        settings.sound_type
    }
}

// String setter helpers for C compatibility
#[no_mangle]
pub extern "C" fn settings_set_sound_type(settings: *mut Settings, value: *const c_char) {
    if settings.is_null() {
        return;
    }
    
    unsafe {
        let settings = &mut *settings;
        
        // Free old string if any
        if !settings.sound_type.is_null() {
            drop(CString::from_raw(settings.sound_type));
        }
        
        // Set new value
        if value.is_null() {
            settings.sound_type = std::ptr::null_mut();
        } else {
            if let Ok(c_str) = CStr::from_ptr(value).to_str() {
                if let Ok(new_str) = CString::new(c_str) {
                    settings.sound_type = new_str.into_raw();
                } else {
                    settings.sound_type = std::ptr::null_mut();
                }
            } else {
                settings.sound_type = std::ptr::null_mut();
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn settings_set_work_start_sound(settings: *mut Settings, value: *const c_char) {
    if settings.is_null() {
        return;
    }
    
    unsafe {
        let settings = &mut *settings;
        
        // Free old string if any
        if !settings.work_start_sound.is_null() {
            drop(CString::from_raw(settings.work_start_sound));
        }
        
        // Set new value
        if value.is_null() {
            settings.work_start_sound = std::ptr::null_mut();
        } else {
            if let Ok(c_str) = CStr::from_ptr(value).to_str() {
                if let Ok(new_str) = CString::new(c_str) {
                    settings.work_start_sound = new_str.into_raw();
                } else {
                    settings.work_start_sound = std::ptr::null_mut();
                }
            } else {
                settings.work_start_sound = std::ptr::null_mut();
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn settings_set_break_start_sound(settings: *mut Settings, value: *const c_char) {
    if settings.is_null() {
        return;
    }
    
    unsafe {
        let settings = &mut *settings;
        
        // Free old string if any
        if !settings.break_start_sound.is_null() {
            drop(CString::from_raw(settings.break_start_sound));
        }
        
        // Set new value
        if value.is_null() {
            settings.break_start_sound = std::ptr::null_mut();
        } else {
            if let Ok(c_str) = CStr::from_ptr(value).to_str() {
                if let Ok(new_str) = CString::new(c_str) {
                    settings.break_start_sound = new_str.into_raw();
                } else {
                    settings.break_start_sound = std::ptr::null_mut();
                }
            } else {
                settings.break_start_sound = std::ptr::null_mut();
            }
        }
    }
}

// Free C strings allocated by Rust
#[no_mangle]
pub extern "C" fn rust_free_string(s: *mut c_char) {
    if s.is_null() {
        return;
    }
    
    unsafe {
        drop(CString::from_raw(s));
    }
}