#include "input_monitor.h"
#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

struct _InputMonitor {
    gboolean is_active;
    InputMonitorCallback callback;
    gpointer user_data;
    
    // Simplified idle monitoring
    guint idle_timer_id;
    int last_idle_time;
    int activity_threshold;  // seconds of inactivity before considering idle
    int consecutive_failures;  // Track consecutive query failures
    Display *shared_display;   // Reuse display connection
};

static gboolean check_activity_timeout(gpointer user_data);
static gboolean trigger_callback_idle(gpointer user_data);

InputMonitor* input_monitor_new(void) {
    InputMonitor *monitor = g_malloc0(sizeof(InputMonitor));
    
    monitor->is_active = FALSE;
    monitor->callback = NULL;
    monitor->user_data = NULL;
    monitor->idle_timer_id = 0;
    monitor->last_idle_time = 0;
    monitor->activity_threshold = 2;  // 2 seconds of activity triggers callback
    monitor->consecutive_failures = 0;
    monitor->shared_display = NULL;
    
    return monitor;
}

void input_monitor_free(InputMonitor *monitor) {
    if (!monitor) return;
    
    input_monitor_stop(monitor);
    
    // Close shared display if open
    if (monitor->shared_display) {
        XCloseDisplay(monitor->shared_display);
        monitor->shared_display = NULL;
    }
    
    g_free(monitor);
}

void input_monitor_set_callback(InputMonitor *monitor, InputMonitorCallback callback, gpointer user_data) {
    if (!monitor) return;
    
    monitor->callback = callback;
    monitor->user_data = user_data;
}


void input_monitor_start(InputMonitor *monitor) {
    if (!monitor) {
        g_print("Input monitor: start called with NULL monitor\n");
        return;
    }
    
    if (monitor->is_active) {
        g_print("Input monitor: already active, not starting again\n");
        return;
    }
    
    g_print("Input monitor: starting idle monitoring (checking every 250ms)\n");
    monitor->is_active = TRUE;
    
    // Get initial idle time in milliseconds
    monitor->last_idle_time = input_monitor_get_idle_time(monitor);
    if (monitor->last_idle_time < 0) {
        monitor->last_idle_time = 0;  // Start from 0 if we can't get initial value
    }
    g_print("Input monitor: initial idle time = %d.%03d seconds\n", 
            monitor->last_idle_time / 1000, monitor->last_idle_time % 1000);
    
    // Check for activity every 250ms for better responsiveness
    monitor->idle_timer_id = g_timeout_add(250, check_activity_timeout, monitor);
}

void input_monitor_stop(InputMonitor *monitor) {
    if (!monitor || !monitor->is_active) return;
    
    g_print("Input monitor: stopping monitoring\n");
    
    // Remove the timer
    if (monitor->idle_timer_id) {
        g_source_remove(monitor->idle_timer_id);
        monitor->idle_timer_id = 0;
    }
    
    monitor->is_active = FALSE;
}

gboolean input_monitor_is_active(InputMonitor *monitor) {
    if (!monitor) return FALSE;
    return monitor->is_active;
}

static gboolean check_activity_timeout(gpointer user_data) {
    InputMonitor *monitor = (InputMonitor*)user_data;
    
    if (!monitor->is_active) {
        return G_SOURCE_REMOVE;
    }
    
    int current_idle_ms = input_monitor_get_idle_time(monitor);
    
    // If we can't get idle time, continue checking but don't trigger
    if (current_idle_ms < 0) {
        // After too many failures, stop trying
        if (monitor->consecutive_failures > 20) {
            g_warning("Input monitor: too many failures, stopping");
            monitor->is_active = FALSE;
            return G_SOURCE_REMOVE;
        }
        return G_SOURCE_CONTINUE;
    }
    
    // Convert to seconds for logging
    int current_idle_sec = current_idle_ms / 1000;
    int last_idle_sec = monitor->last_idle_time / 1000;
    
    // Detect activity: idle time reset to near zero (< 500ms)
    // This catches when user moves mouse/types after being idle
    if (monitor->last_idle_time > 2000 && current_idle_ms < 500) {
        g_print("Input monitor: activity detected! (idle: %d.%03ds -> %d.%03ds)\n", 
                last_idle_sec, monitor->last_idle_time % 1000,
                current_idle_sec, current_idle_ms % 1000);
        
        // Stop monitoring and trigger callback
        monitor->is_active = FALSE;
        
        if (monitor->callback) {
            g_idle_add(trigger_callback_idle, monitor);
        }
        
        return G_SOURCE_REMOVE;
    }
    
    // Also detect significant decreases in idle time (> 1 second drop)
    // This handles cases where the idle timer doesn't fully reset
    if (current_idle_ms < monitor->last_idle_time - 1000) {
        g_print("Input monitor: activity detected (significant decrease)! (idle: %d.%03ds -> %d.%03ds)\n", 
                last_idle_sec, monitor->last_idle_time % 1000,
                current_idle_sec, current_idle_ms % 1000);
        
        // Stop monitoring and trigger callback
        monitor->is_active = FALSE;
        
        if (monitor->callback) {
            g_idle_add(trigger_callback_idle, monitor);
        }
        
        return G_SOURCE_REMOVE;
    }
    
    monitor->last_idle_time = current_idle_ms;
    return G_SOURCE_CONTINUE;
}

static gboolean trigger_callback_idle(gpointer user_data) {
    InputMonitor *monitor = (InputMonitor*)user_data;
    
    g_print("Input monitor: trigger_callback_idle called, is_active=%d\n", monitor->is_active);
    
    if (monitor->callback) {
        monitor->callback(monitor->user_data);
    }
    
    return G_SOURCE_REMOVE;
}

int input_monitor_get_idle_time(InputMonitor *monitor) {
    // Try to reuse existing display connection
    Display *display = monitor ? monitor->shared_display : NULL;
    gboolean created_display = FALSE;
    
    if (!display) {
        display = XOpenDisplay(NULL);
        if (!display) {
            if (monitor) monitor->consecutive_failures++;
            // Only warn on first failure or every 10th failure to avoid log spam
            if (!monitor || monitor->consecutive_failures == 1 || monitor->consecutive_failures % 10 == 0) {
                g_warning("Failed to open X11 display for idle time detection (failures: %d)",
                         monitor ? monitor->consecutive_failures : 0);
            }
            return -1;
        }
        created_display = TRUE;
        
        // Store for reuse if we have a monitor
        if (monitor && !monitor->shared_display) {
            monitor->shared_display = display;
            created_display = FALSE;  // Don't close it later
        }
    }
    
    static int event_base = -1, error_base = -1;
    static gboolean extension_checked = FALSE;
    
    // Check extension only once
    if (!extension_checked) {
        if (!XScreenSaverQueryExtension(display, &event_base, &error_base)) {
            g_warning("XScreenSaver extension not available - idle detection will not work");
            if (created_display) XCloseDisplay(display);
            return -1;
        }
        extension_checked = TRUE;
    }
    
    XScreenSaverInfo *info = XScreenSaverAllocInfo();
    if (!info) {
        g_warning("Failed to allocate XScreenSaverInfo");
        if (created_display) XCloseDisplay(display);
        return -1;
    }
    
    // Use XSync to ensure we get the latest state
    XSync(display, False);
    
    if (!XScreenSaverQueryInfo(display, DefaultRootWindow(display), info)) {
        if (monitor) monitor->consecutive_failures++;
        if (!monitor || monitor->consecutive_failures == 1 || monitor->consecutive_failures % 10 == 0) {
            g_warning("Failed to query idle time (failures: %d)",
                     monitor ? monitor->consecutive_failures : 0);
        }
        XFree(info);
        if (created_display) XCloseDisplay(display);
        return -1;
    }
    
    // Reset failure counter on success
    if (monitor) monitor->consecutive_failures = 0;
    
    int idle_milliseconds = info->idle;
    XFree(info);
    
    // Close display only if we created it for this call
    if (created_display) {
        XCloseDisplay(display);
    }
    
    // Return milliseconds for more precision
    return idle_milliseconds;
}