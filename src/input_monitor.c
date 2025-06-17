#include "input_monitor.h"
#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/scrnsaver.h>
#include <pthread.h>
#include <unistd.h>

struct _InputMonitor {
    gboolean is_active;
    InputMonitorCallback callback;
    gpointer user_data;
    GtkWidget *window;
    gulong signal_handler_id;
    
    // X11 global input monitoring
    Display *x11_display;
    pthread_t x11_thread;
    gboolean x11_thread_running;
    gboolean use_global_monitoring;
    int xi_opcode;  // XInput2 opcode for event filtering
};

static gboolean on_window_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static void* x11_monitor_thread(void *data);
static gboolean trigger_callback_idle(gpointer user_data);
static gboolean setup_x11_monitoring(InputMonitor *monitor);
static void cleanup_x11_monitoring(InputMonitor *monitor);

InputMonitor* input_monitor_new(void) {
    InputMonitor *monitor = g_malloc0(sizeof(InputMonitor));
    
    monitor->is_active = FALSE;
    monitor->callback = NULL;
    monitor->user_data = NULL;
    monitor->window = NULL;
    monitor->signal_handler_id = 0;
    
    // X11 global monitoring
    monitor->x11_display = NULL;
    monitor->x11_thread_running = FALSE;
    monitor->use_global_monitoring = TRUE;  // Default to global monitoring
    monitor->xi_opcode = 0;
    
    return monitor;
}

void input_monitor_free(InputMonitor *monitor) {
    if (!monitor) return;
    
    input_monitor_stop(monitor);
    cleanup_x11_monitoring(monitor);
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
    if (!monitor) {
        g_print("Input monitor: start called with NULL monitor\n");
        return;
    }
    
    if (monitor->is_active) {
        g_print("Input monitor: already active, not starting again\n");
        return;
    }
    
    g_print("Input monitor: starting monitoring\n");
    monitor->is_active = TRUE;
    
    if (monitor->use_global_monitoring) {
        // Use X11 global input monitoring
        if (setup_x11_monitoring(monitor)) {
            g_print("Global input monitoring started successfully\n");
        } else {
            // Fallback to window-based monitoring
            g_print("Failed to setup global monitoring, using window-based fallback\n");
            monitor->use_global_monitoring = FALSE;
            
            if (monitor->window) {
                monitor->signal_handler_id = g_signal_connect(monitor->window, "event", 
                                                             G_CALLBACK(on_window_event), monitor);
            }
        }
    } else if (monitor->window) {
        // Use GTK window event monitoring (fallback)
        monitor->signal_handler_id = g_signal_connect(monitor->window, "event", 
                                                     G_CALLBACK(on_window_event), monitor);
    }
}

void input_monitor_stop(InputMonitor *monitor) {
    if (!monitor || !monitor->is_active) return;
    
    // Stop X11 global monitoring
    if (monitor->use_global_monitoring) {
        cleanup_x11_monitoring(monitor);
    }
    
    // Disconnect the GTK signal handler if using window monitoring
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
            g_idle_add(trigger_callback_idle, monitor);
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

// X11 Global Input Monitoring Implementation

static gboolean trigger_callback_idle(gpointer user_data) {
    InputMonitor *monitor = (InputMonitor*)user_data;
    
    g_print("Input monitor: trigger_callback_idle called, is_active=%d\n", monitor->is_active);
    
    if (monitor->callback) {
        monitor->callback(monitor->user_data);
    }
    
    return G_SOURCE_REMOVE;
}

static void* x11_monitor_thread(void *data) {
    InputMonitor *monitor = (InputMonitor*)data;
    Display *display = monitor->x11_display;
    
    if (!display) {
        return NULL;
    }
    
    // Set up XInput2 for raw input events
    XIEventMask evmask;
    unsigned char mask[(XI_LASTEVENT + 7)/8] = { 0 };
    
    XISetMask(mask, XI_RawKeyPress);
    XISetMask(mask, XI_RawButtonPress);
    XISetMask(mask, XI_RawMotion);
    
    evmask.deviceid = XIAllMasterDevices;
    evmask.mask_len = sizeof(mask);
    evmask.mask = mask;
    
    if (XISelectEvents(display, DefaultRootWindow(display), &evmask, 1) != Success) {
        g_warning("Failed to select XInput2 events");
        return NULL;
    }
    
    // Event loop
    while (monitor->x11_thread_running) {
        if (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            
            if (event.type == GenericEvent && event.xcookie.extension == monitor->xi_opcode) {
                if (XGetEventData(display, &event.xcookie)) {
                    XIRawEvent *raw_event = (XIRawEvent*)event.xcookie.data;
                    
                    // Check for activity (keyboard press, mouse button, or significant mouse movement)
                    if (raw_event->evtype == XI_RawKeyPress || 
                        raw_event->evtype == XI_RawButtonPress ||
                        raw_event->evtype == XI_RawMotion) {
                        
                        // For mouse motion, only trigger on significant movement
                        if (raw_event->evtype == XI_RawMotion) {
                            // Simple threshold to avoid triggering on tiny movements
                            static double last_x = 0, last_y = 0;
                            double curr_x = 0, curr_y = 0;
                            
                            // Get current mouse position (simplified)
                            if (raw_event->valuators.mask_len > 0) {
                                if (XIMaskIsSet(raw_event->valuators.mask, 0)) curr_x = raw_event->raw_values[0];
                                if (XIMaskIsSet(raw_event->valuators.mask, 1)) curr_y = raw_event->raw_values[1];
                                
                                double dx = curr_x - last_x;
                                double dy = curr_y - last_y;
                                double distance = dx*dx + dy*dy;
                                
                                // Only trigger on movements > 10 pixels
                                if (distance < 100) {
                                    XFreeEventData(display, &event.xcookie);
                                    continue;
                                }
                                
                                last_x = curr_x;
                                last_y = curr_y;
                            }
                        }
                        
                        // Activity detected - trigger callback in main thread
                        g_print("X11 input monitor: activity detected! Triggering callback\n");
                        g_idle_add(trigger_callback_idle, monitor);
                        monitor->x11_thread_running = FALSE; // Stop monitoring
                        break;
                    }
                    
                    XFreeEventData(display, &event.xcookie);
                }
            }
        } else {
            // Small sleep to avoid busy waiting
            usleep(10000); // 10ms
        }
    }
    
    return NULL;
}

static gboolean setup_x11_monitoring(InputMonitor *monitor) {
    // Try to connect to X11 display
    monitor->x11_display = XOpenDisplay(NULL);
    if (!monitor->x11_display) {
        g_warning("Failed to open X11 display for global input monitoring");
        return FALSE;
    }
    
    // Check for XInput2 extension
    int event, error;
    if (!XQueryExtension(monitor->x11_display, "XInputExtension", &monitor->xi_opcode, &event, &error)) {
        g_warning("XInput extension not available");
        XCloseDisplay(monitor->x11_display);
        monitor->x11_display = NULL;
        return FALSE;
    }
    
    int major = 2, minor = 0;
    if (XIQueryVersion(monitor->x11_display, &major, &minor) != Success) {
        g_warning("XInput2 not available");
        XCloseDisplay(monitor->x11_display);
        monitor->x11_display = NULL;
        return FALSE;
    }
    
    // Start monitoring thread
    monitor->x11_thread_running = TRUE;
    if (pthread_create(&monitor->x11_thread, NULL, x11_monitor_thread, monitor) != 0) {
        g_warning("Failed to create X11 monitoring thread");
        monitor->x11_thread_running = FALSE;
        XCloseDisplay(monitor->x11_display);
        monitor->x11_display = NULL;
        return FALSE;
    }
    
    return TRUE;
}

static void cleanup_x11_monitoring(InputMonitor *monitor) {
    if (monitor->x11_thread_running) {
        monitor->x11_thread_running = FALSE;
        
        // Wait for thread to finish
        pthread_join(monitor->x11_thread, NULL);
    }
    
    if (monitor->x11_display) {
        XCloseDisplay(monitor->x11_display);
        monitor->x11_display = NULL;
    }
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