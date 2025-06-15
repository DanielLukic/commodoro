#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include <gtk/gtk.h>
#include <cairo.h>
#include "timer.h"

G_BEGIN_DECLS

typedef struct _TrayIcon TrayIcon;

/**
 * Creates a new tray icon instance
 * @return New TrayIcon object
 */
TrayIcon* tray_icon_new(void);

/**
 * Frees a tray icon instance
 * @param self TrayIcon instance to free
 */
void tray_icon_free(TrayIcon *self);

/**
 * Updates the tray icon with timer state and time
 * @param self TrayIcon instance
 * @param state Current timer state
 * @param remaining_seconds Seconds remaining in current timer
 * @param total_seconds Total seconds for current timer state
 */
void tray_icon_update(TrayIcon *self, TimerState state, int remaining_seconds, int total_seconds);

/**
 * Sets the tooltip text for the tray icon
 * @param self TrayIcon instance  
 * @param tooltip Tooltip text to display
 */
void tray_icon_set_tooltip(TrayIcon *self, const char *tooltip);

/**
 * Checks if the tray icon is embedded in the system tray
 * @param self TrayIcon instance
 * @return TRUE if embedded, FALSE otherwise
 */
gboolean tray_icon_is_embedded(TrayIcon *self);

/**
 * Gets the Cairo surface containing the rendered icon
 * @param self TrayIcon instance
 * @return Cairo surface with the icon, or NULL on error
 */
cairo_surface_t* tray_icon_get_surface(TrayIcon *self);

G_END_DECLS

#endif // TRAY_ICON_H