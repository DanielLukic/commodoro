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
    
    return monitor;
}

void input_monitor_free(InputMonitor *monitor) {
    if (!monitor) return;
    
    input_monitor_stop(monitor);
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
    
    g_print("Input monitor: starting idle monitoring (checking every 500ms)\n");
    monitor->is_active = TRUE;
    monitor->last_idle_time = input_monitor_get_idle_time(monitor);
    
    // Check for activity every 500ms
    monitor->idle_timer_id = g_timeout_add(500, check_activity_timeout, monitor);
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
    
    int current_idle_time = input_monitor_get_idle_time(monitor);
    
    // If we can't get idle time, continue checking
    if (current_idle_time < 0) {
        return G_SOURCE_CONTINUE;
    }
    
    // Check if user became active (idle time decreased significantly)
    if (current_idle_time < monitor->last_idle_time - 1) {
        g_print("Input monitor: activity detected! (idle time: %d -> %d)\n", 
                monitor->last_idle_time, current_idle_time);
        
        // Stop monitoring and trigger callback
        monitor->is_active = FALSE;
        
        if (monitor->callback) {
            g_idle_add(trigger_callback_idle, monitor);
        }
        
        return G_SOURCE_REMOVE;
    }
    
    monitor->last_idle_time = current_idle_time;
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
    (void)monitor; // Not needed for this function
    
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        g_warning("Failed to open X11 display for idle time detection");
        return -1;
    }
    
    int event_base, error_base;
    if (!XScreenSaverQueryExtension(display, &event_base, &error_base)) {
        g_warning("XScreenSaver extension not available");
        XCloseDisplay(display);
        return -1;
    }
    
    XScreenSaverInfo *info = XScreenSaverAllocInfo();
    if (!info) {
        g_warning("Failed to allocate XScreenSaverInfo");
        XCloseDisplay(display);
        return -1;
    }
    
    if (!XScreenSaverQueryInfo(display, DefaultRootWindow(display), info)) {
        g_warning("Failed to query idle time");
        XFree(info);
        XCloseDisplay(display);
        return -1;
    }
    
    int idle_seconds = info->idle / 1000;  // Convert milliseconds to seconds
    
    XFree(info);
    XCloseDisplay(display);
    
    return idle_seconds;
}