#ifndef SYSTEM_TRAY_H
#define SYSTEM_TRAY_H

#include <glib.h>
#include <cairo.h>

G_BEGIN_DECLS

typedef struct _SystemTray SystemTray;

/**
 * Callback function for tray menu actions
 * @param action Action name (e.g., "show", "start", "pause", "reset", "quit")
 * @param user_data User data passed to callback
 */
typedef void (*SystemTrayCallback)(const char *action, gpointer user_data);

/**
 * Creates a new system tray instance
 * @return New SystemTray object
 */
SystemTray* system_tray_new(void);

/**
 * Frees a system tray instance
 * @param tray SystemTray instance to free
 */
void system_tray_free(SystemTray *tray);

/**
 * Sets the callback for tray menu actions
 * @param tray SystemTray instance
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void system_tray_set_callback(SystemTray *tray, SystemTrayCallback callback, gpointer user_data);

/**
 * Updates the tray icon with new icon surface and tooltip
 * @param tray SystemTray instance
 * @param surface Cairo surface with rendered icon
 * @param tooltip Tooltip text
 */
void system_tray_update(SystemTray *tray, cairo_surface_t *surface, const char *tooltip);

/**
 * Updates the tray menu state (enabled/disabled items)
 * @param tray SystemTray instance
 * @param can_start Whether start action is available
 * @param can_pause Whether pause action is available
 * @param can_reset Whether reset action is available
 */
void system_tray_update_menu(SystemTray *tray, gboolean can_start, gboolean can_pause, gboolean can_reset);

G_END_DECLS

#endif // SYSTEM_TRAY_H