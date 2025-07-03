#ifndef INPUT_MONITOR_H
#define INPUT_MONITOR_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _InputMonitor InputMonitor;

/**
 * Callback function for activity detection (compatible with GSourceFunc)
 * @param user_data User data passed to callback
 * @return FALSE to remove the source (G_SOURCE_REMOVE)
 */
typedef gboolean (*InputMonitorCallback)(gpointer user_data);

/**
 * Creates a new input monitor instance
 * @return New InputMonitor object
 */
InputMonitor* input_monitor_new(void);

/**
 * Frees an input monitor instance
 * @param monitor InputMonitor instance to free
 */
void input_monitor_free(InputMonitor *monitor);

/**
 * Sets the callback for activity detection
 * @param monitor InputMonitor instance
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void input_monitor_set_callback(InputMonitor *monitor, InputMonitorCallback callback, gpointer user_data);

/**
 * Sets the window for GTK event-based input monitoring
 * @param monitor InputMonitor instance
 * @param window GTK window to monitor for events
 */
void input_monitor_set_window(InputMonitor *monitor, GtkWidget *window);

/**
 * Starts monitoring user input activity
 * @param monitor InputMonitor instance
 */
void input_monitor_start(InputMonitor *monitor);

/**
 * Stops monitoring user input activity
 * @param monitor InputMonitor instance
 */
void input_monitor_stop(InputMonitor *monitor);

/**
 * Checks if monitoring is active
 * @param monitor InputMonitor instance
 * @return TRUE if monitoring, FALSE otherwise
 */
gboolean input_monitor_is_active(InputMonitor *monitor);

/**
 * Gets the current system idle time in seconds
 * @param monitor InputMonitor instance
 * @return Idle time in seconds, or -1 on error
 */
int input_monitor_get_idle_time(InputMonitor *monitor);

G_END_DECLS

#endif // INPUT_MONITOR_H