#include "input_monitor.h"
#include <stdio.h>
#include <gtk/gtk.h>

struct _InputMonitor {
    gboolean is_active;
    InputMonitorCallback callback;
    gpointer user_data;
    GtkWidget *window;
    gulong signal_handler_id;
};

static gboolean on_window_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);

InputMonitor* input_monitor_new(void) {
    InputMonitor *monitor = g_malloc0(sizeof(InputMonitor));
    
    monitor->is_active = FALSE;
    monitor->callback = NULL;
    monitor->user_data = NULL;
    monitor->window = NULL;
    monitor->signal_handler_id = 0;
    
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

void input_monitor_set_window(InputMonitor *monitor, GtkWidget *window) {
    if (!monitor) return;
    monitor->window = window;
}

void input_monitor_start(InputMonitor *monitor) {
    if (!monitor || monitor->is_active || !monitor->window) return;
    
    
    monitor->is_active = TRUE;
    
    // Connect to window's event signal to detect any user activity
    monitor->signal_handler_id = g_signal_connect(monitor->window, "event", 
                                                 G_CALLBACK(on_window_event), monitor);
}

void input_monitor_stop(InputMonitor *monitor) {
    if (!monitor || !monitor->is_active) return;
    
    
    // Disconnect the signal handler
    if (monitor->signal_handler_id && monitor->window) {
        g_signal_handler_disconnect(monitor->window, monitor->signal_handler_id);
        monitor->signal_handler_id = 0;
    }
    
    monitor->is_active = FALSE;
}

gboolean input_monitor_is_active(InputMonitor *monitor) {
    if (!monitor) return FALSE;
    return monitor->is_active;
}

static gboolean on_window_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)widget; // Suppress unused parameter warning
    InputMonitor *monitor = (InputMonitor*)user_data;
    
    if (!monitor->is_active) return FALSE;
    
    // Check for user activity events (same as working pymodoro implementation)
    if (event->type == GDK_KEY_PRESS || 
        event->type == GDK_BUTTON_PRESS || 
        event->type == GDK_MOTION_NOTIFY) {
        
        
        // Stop monitoring immediately
        monitor->is_active = FALSE;
        
        // Trigger callback using g_idle_add 
        if (monitor->callback) {
            g_idle_add(monitor->callback, monitor->user_data);
        }
        
        // Disconnect signal handler
        if (monitor->signal_handler_id) {
            g_signal_handler_disconnect(monitor->window, monitor->signal_handler_id);
            monitor->signal_handler_id = 0;
        }
        
        return TRUE; // Event handled
    }
    
    return FALSE; // Let other handlers process the event
}