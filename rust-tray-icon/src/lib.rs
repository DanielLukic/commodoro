use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_void};
use cairo::{Context, Format, ImageSurface};
use std::f64::consts::PI;

// Import TimerState from the timer module
use commodoro_timer::TimerState;

#[repr(C)]
pub struct TrayIcon {
    icon_surface: *mut c_void, // cairo_surface_t*
    size: i32,
    state: TimerState,
    remaining_seconds: i32,
    total_seconds: i32,
    tooltip_text: *mut c_char,
}

// State colors matching Python implementation
#[derive(Copy, Clone)]
struct StateColor {
    r: f64,
    g: f64,
    b: f64,
}

const STATE_COLORS: [StateColor; 5] = [
    StateColor { r: 0.5, g: 0.5, b: 0.5 },     // IDLE - Gray
    StateColor { r: 0.86, g: 0.20, b: 0.18 },  // WORK - Red
    StateColor { r: 0.18, g: 0.49, b: 0.20 },  // SHORT_BREAK - Green
    StateColor { r: 0.18, g: 0.49, b: 0.20 },  // LONG_BREAK - Green
    StateColor { r: 0.71, g: 0.54, b: 0.0 },   // PAUSED - Yellow
];

impl TrayIcon {
    fn new() -> Self {
        let mut icon = TrayIcon {
            icon_surface: std::ptr::null_mut(),
            size: 64,
            state: TimerState::Idle,
            remaining_seconds: 0,
            total_seconds: 0,
            tooltip_text: CString::new("Commodoro Timer").unwrap().into_raw(),
        };
        
        icon.update_icon_surface();
        icon
    }
    
    fn update_icon_surface(&mut self) {
        // Destroy old surface if it exists
        if !self.icon_surface.is_null() {
            unsafe {
                cairo_sys::cairo_surface_destroy(self.icon_surface as *mut cairo_sys::cairo_surface_t);
            }
        }
        
        // Create new surface
        let surface = ImageSurface::create(Format::ARgb32, self.size, self.size)
            .expect("Failed to create surface");
        
        let cr = Context::new(&surface).expect("Failed to create cairo context");
        
        // Clear background with transparent
        cr.set_operator(cairo::Operator::Clear);
        cr.paint().expect("Failed to clear");
        cr.set_operator(cairo::Operator::Over);
        
        let center_x = self.size as f64 / 2.0;
        let center_y = self.size as f64 / 2.0;
        let radius = (self.size as f64 - 4.0) / 2.0; // Leave 2px margin
        
        // Get state color
        let color = STATE_COLORS[self.state as usize];
        
        // Draw colored circle background
        cr.set_source_rgba(color.r, color.g, color.b, 1.0);
        cr.arc(center_x, center_y, radius, 0.0, 2.0 * PI);
        cr.fill().expect("Failed to fill");
        
        // Draw progress arc if there's remaining time
        if self.state != TimerState::Idle && self.state != TimerState::Paused && self.total_seconds > 0 {
            let progress = self.get_progress();
            if progress > 0.0 {
                cr.set_line_width(self.size as f64 * 0.15); // 15% of icon size for border
                
                // Set progress arc color based on state (inverse colors)
                if self.state == TimerState::Work {
                    cr.set_source_rgba(0.18, 0.49, 0.20, 1.0); // Green border for work (red bg)
                } else {
                    // Break states
                    cr.set_source_rgba(0.86, 0.20, 0.18, 1.0); // Red border for breaks (green bg)
                }
                
                // Draw arc from top, clockwise
                let start_angle = -PI / 2.0; // Start at top (90 degrees)
                let span_angle = progress * 2.0 * PI; // Positive for clockwise
                
                let margin = self.size as f64 * 0.1; // 10% margin
                let arc_radius = (self.size as f64 - 2.0 * margin) / 2.0;
                
                cr.arc(center_x, center_y, arc_radius, start_angle, start_angle + span_angle);
                cr.stroke().expect("Failed to stroke");
            }
        }
        
        // Draw text based on state
        cr.set_source_rgba(1.0, 1.0, 1.0, 1.0); // White text
        
        let text = match self.state {
            TimerState::Idle => "●".to_string(),
            TimerState::Paused => "||".to_string(),
            _ => {
                // Show rounded minutes
                let minutes = self.get_rounded_minutes();
                minutes.to_string()
            }
        };
        
        // Set font (bold, larger text for better visibility)
        cr.select_font_face("Sans", cairo::FontSlant::Normal, cairo::FontWeight::Bold);
        cr.set_font_size(self.size as f64 * 0.4); // 40% of icon size
        
        // Center the text
        let extents = cr.text_extents(&text).expect("Failed to get text extents");
        let text_x = center_x - (extents.width() / 2.0 + extents.x_bearing());
        let text_y = center_y - (extents.height() / 2.0 + extents.y_bearing());
        
        cr.move_to(text_x, text_y);
        cr.show_text(&text).expect("Failed to show text");
        
        // Ensure all drawing operations are finished
        surface.flush();
        
        // Store the new surface
        // We need to store the actual cairo_surface_t pointer that C code can use
        // The cairo-rs Surface type wraps the raw pointer, we need to extract it
        self.icon_surface = unsafe {
            use cairo::ffi::cairo_surface_t;
            // Get a reference-counted raw pointer
            let ptr = surface.to_raw_none();
            cairo_sys::cairo_surface_reference(ptr as *mut cairo_surface_t);
            ptr as *mut c_void
        };
    }
    
    fn get_rounded_minutes(&self) -> i32 {
        let minutes = self.remaining_seconds / 60;
        let seconds = self.remaining_seconds % 60;
        
        // Round up if seconds >= 30 (like Python version)
        if seconds >= 30 {
            minutes + 1
        } else {
            minutes
        }
    }
    
    fn get_progress(&self) -> f64 {
        if self.total_seconds <= 0 {
            return 0.0;
        }
        
        // Return progress as elapsed time ratio (starts at 0.0, increases to 1.0)
        let elapsed = self.total_seconds - self.remaining_seconds;
        elapsed as f64 / self.total_seconds as f64
    }
}

// C FFI exports
#[no_mangle]
pub extern "C" fn tray_icon_new() -> *mut TrayIcon {
    Box::into_raw(Box::new(TrayIcon::new()))
}

#[no_mangle]
pub extern "C" fn tray_icon_free(icon: *mut TrayIcon) {
    if icon.is_null() {
        return;
    }
    unsafe {
        let icon = Box::from_raw(icon);
        
        // Free the Cairo surface
        if !icon.icon_surface.is_null() {
            cairo_sys::cairo_surface_destroy(icon.icon_surface as *mut cairo_sys::cairo_surface_t);
        }
        
        // Free the tooltip text
        if !icon.tooltip_text.is_null() {
            let _ = CString::from_raw(icon.tooltip_text);
        }
        
        drop(icon);
    }
}

#[no_mangle]
pub extern "C" fn tray_icon_update(
    icon: *mut TrayIcon,
    state: TimerState,
    remaining_seconds: c_int,
    total_seconds: c_int,
) {
    if icon.is_null() {
        return;
    }
    
    unsafe {
        let icon = &mut *icon;
        icon.state = state;
        icon.remaining_seconds = remaining_seconds;
        icon.total_seconds = total_seconds;
        icon.update_icon_surface();
    }
}

#[no_mangle]
pub extern "C" fn tray_icon_set_tooltip(icon: *mut TrayIcon, tooltip: *const c_char) {
    if icon.is_null() || tooltip.is_null() {
        return;
    }
    
    unsafe {
        let icon = &mut *icon;
        
        // Free old tooltip
        if !icon.tooltip_text.is_null() {
            let _ = CString::from_raw(icon.tooltip_text);
        }
        
        // Set new tooltip
        let tooltip_str = CStr::from_ptr(tooltip);
        icon.tooltip_text = CString::new(tooltip_str.to_bytes()).unwrap().into_raw();
    }
}

#[no_mangle]
pub extern "C" fn tray_icon_is_embedded(_icon: *mut TrayIcon) -> c_int {
    // For now, always return FALSE since we don't have actual tray integration
    // This will be implemented later with proper system tray support
    0
}

#[no_mangle]
pub extern "C" fn tray_icon_get_surface(icon: *mut TrayIcon) -> *mut c_void {
    if icon.is_null() {
        return std::ptr::null_mut();
    }
    
    unsafe {
        let icon = &*icon;
        if icon.icon_surface.is_null() {
            return std::ptr::null_mut();
        }
        
        // Return the stored cairo_surface_t pointer directly
        icon.icon_surface
    }
}