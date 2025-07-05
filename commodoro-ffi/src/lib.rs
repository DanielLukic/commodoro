// Re-export all FFI functions from timer, config, and audio modules
pub use commodoro_timer::*;
pub use commodoro_config::*;
pub use commodoro_audio::*;
// Re-export tray icon functions but not TimerState (which comes from timer)
pub use commodoro_tray_icon::{
    TrayIcon,
    tray_icon_new,
    tray_icon_free,
    tray_icon_update,
    tray_icon_set_tooltip,
    tray_icon_is_embedded,
    tray_icon_get_surface,
};
pub use commodoro_break_overlay::*;
pub use commodoro_settings_dialog::*;