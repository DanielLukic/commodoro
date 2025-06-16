#include "tray_icon.h"
#include <cairo.h>
#include <math.h>
#include <stdio.h>

struct _TrayIcon {
    cairo_surface_t *icon_surface;
    int size;
    TimerState state;
    int remaining_seconds;
    int total_seconds;
    char *tooltip_text;
};

// State colors matching Python implementation
typedef struct {
    double r, g, b;
} StateColor;

static const StateColor state_colors[] = {
    [TIMER_STATE_IDLE] = {0.5, 0.5, 0.5},          // Gray
    [TIMER_STATE_WORK] = {0.86, 0.20, 0.18},       // Red  
    [TIMER_STATE_SHORT_BREAK] = {0.18, 0.49, 0.20}, // Green (inverse of work: green bg)
    [TIMER_STATE_LONG_BREAK] = {0.18, 0.49, 0.20},  // Green (same as short break)
    [TIMER_STATE_PAUSED] = {0.71, 0.54, 0.0}        // Yellow
};

static void update_icon_surface(TrayIcon *self);
static int get_rounded_minutes(int remaining_seconds);
static double get_progress(int remaining_seconds, int total_seconds);

static void update_icon_surface(TrayIcon *self) {
    if (self->icon_surface) {
        cairo_surface_destroy(self->icon_surface);
    }
    
    // Create new surface (64x64 like Python version)
    self->icon_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, self->size, self->size);
    cairo_t *cr = cairo_create(self->icon_surface);
    
    // Clear background with transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    
    double center_x = self->size / 2.0;
    double center_y = self->size / 2.0;
    double radius = (self->size - 4) / 2.0; // Leave 2px margin
    
    // Get state color
    StateColor color = state_colors[self->state];
    
    // Draw colored circle background
    cairo_set_source_rgba(cr, color.r, color.g, color.b, 1.0);
    cairo_arc(cr, center_x, center_y, radius, 0, 2 * G_PI);
    cairo_fill(cr);
    
    // Draw progress arc if there's remaining time
    if (self->state != TIMER_STATE_IDLE && self->state != TIMER_STATE_PAUSED && self->total_seconds > 0) {
        double progress = get_progress(self->remaining_seconds, self->total_seconds);
        if (progress > 0.0) {
            cairo_set_line_width(cr, self->size * 0.15); // 15% of icon size for border
            
            // Set progress arc color based on state (inverse colors)
            if (self->state == TIMER_STATE_WORK) {
                cairo_set_source_rgba(cr, 0.18, 0.49, 0.20, 1.0); // Green border for work (red bg)
            } else { // Break states
                cairo_set_source_rgba(cr, 0.86, 0.20, 0.18, 1.0); // Red border for breaks (green bg)
            }
            
            // Draw arc from top, clockwise
            double start_angle = -G_PI / 2; // Start at top (90 degrees)
            double span_angle = progress * 2 * G_PI; // Positive for clockwise
            
            double margin = self->size * 0.1; // 10% margin
            double arc_radius = (self->size - 2 * margin) / 2.0;
            
            cairo_arc(cr, center_x, center_y, arc_radius, start_angle, start_angle + span_angle);
            cairo_stroke(cr);
        }
    }
    
    // Draw text based on state
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0); // White text
    
    char text[16];
    if (self->state == TIMER_STATE_IDLE) {
        strcpy(text, "●");
    } else if (self->state == TIMER_STATE_PAUSED) {
        strcpy(text, "||");
    } else {
        // Show rounded minutes
        int minutes = get_rounded_minutes(self->remaining_seconds);
        snprintf(text, sizeof(text), "%d", minutes);
    }
    
    // Set font (bold, larger text for better visibility)
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, self->size * 0.4); // 40% of icon size (twice as big)
    
    // Center the text
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text, &extents);
    double text_x = center_x - (extents.width / 2.0 + extents.x_bearing);
    double text_y = center_y - (extents.height / 2.0 + extents.y_bearing);
    
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, text);
    
    cairo_destroy(cr);
}

static int get_rounded_minutes(int remaining_seconds) {
    int minutes = remaining_seconds / 60;
    int seconds = remaining_seconds % 60;
    
    // Round up if seconds >= 30 (like Python version)
    if (seconds >= 30) {
        minutes += 1;
    }
    
    return minutes;
}

static double get_progress(int remaining_seconds, int total_seconds) {
    if (total_seconds <= 0) {
        return 0.0;
    }
    
    // Return progress as elapsed time ratio (starts at 0.0, increases to 1.0)
    int elapsed = total_seconds - remaining_seconds;
    return (double)elapsed / (double)total_seconds;
}

TrayIcon* tray_icon_new(void) {
    TrayIcon *self = g_malloc0(sizeof(TrayIcon));
    self->size = 64; // Use 64x64 like Python version
    self->state = TIMER_STATE_IDLE;
    self->remaining_seconds = 0;
    self->total_seconds = 0;
    self->icon_surface = NULL;
    self->tooltip_text = g_strdup("Commodoro Timer");
    
    update_icon_surface(self);
    return self;
}

void tray_icon_free(TrayIcon *self) {
    if (!self) return;
    
    if (self->icon_surface) {
        cairo_surface_destroy(self->icon_surface);
    }
    
    g_free(self->tooltip_text);
    g_free(self);
}

void tray_icon_update(TrayIcon *self, TimerState state, int remaining_seconds, int total_seconds) {
    if (!self) return;
    
    self->state = state;
    self->remaining_seconds = remaining_seconds;
    self->total_seconds = total_seconds;
    
    update_icon_surface(self);
}

void tray_icon_set_tooltip(TrayIcon *self, const char *tooltip) {
    if (!self || !tooltip) return;
    
    g_free(self->tooltip_text);
    self->tooltip_text = g_strdup(tooltip);
}

gboolean tray_icon_is_embedded(TrayIcon *self) {
    if (!self) return FALSE;
    
    // For now, always return FALSE since we don't have actual tray integration
    // This will be implemented later with proper system tray support
    return FALSE;
}

cairo_surface_t* tray_icon_get_surface(TrayIcon *self) {
    if (!self) return NULL;
    
    return self->icon_surface;
}