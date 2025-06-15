#ifndef TRAY_STATUS_ICON_H
#define TRAY_STATUS_ICON_H

#include <glib.h>
#include <cairo.h>

G_BEGIN_DECLS

typedef struct _TrayStatusIcon TrayStatusIcon;

/**
 * Callback function for tray icon events
 * @param action Action name (e.g., "activate", "popup-menu")
 * @param user_data User data passed to callback
 */
typedef void (*TrayStatusIconCallback)(const char *action, gpointer user_data);

/**
 * Creates a new tray status icon
 * @return New TrayStatusIcon object
 */
TrayStatusIcon* tray_status_icon_new(void);

/**
 * Frees a tray status icon
 * @param tray TrayStatusIcon instance to free
 */
void tray_status_icon_free(TrayStatusIcon *tray);

/**
 * Sets the callback for tray icon events
 * @param tray TrayStatusIcon instance
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void tray_status_icon_set_callback(TrayStatusIcon *tray, TrayStatusIconCallback callback, gpointer user_data);

/**
 * Updates the tray icon with new surface and tooltip
 * @param tray TrayStatusIcon instance
 * @param surface Cairo surface with rendered icon (32x32)
 * @param tooltip Tooltip text
 */
void tray_status_icon_update(TrayStatusIcon *tray, cairo_surface_t *surface, const char *tooltip);

/**
 * Sets visibility of the tray icon
 * @param tray TrayStatusIcon instance
 * @param visible TRUE to show, FALSE to hide
 */
void tray_status_icon_set_visible(TrayStatusIcon *tray, gboolean visible);

/**
 * Checks if the tray icon is embedded in a system tray
 * @param tray TrayStatusIcon instance
 * @return TRUE if embedded, FALSE otherwise
 */
gboolean tray_status_icon_is_embedded(TrayStatusIcon *tray);

G_END_DECLS

#endif // TRAY_STATUS_ICON_H