#ifndef BREAK_OVERLAY_H
#define BREAK_OVERLAY_H

#include <gtk/gtk.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _BreakOverlay BreakOverlay;

/**
 * Break overlay callback function
 * @param action Action name ("skip_break", "extend_break", "pause", "dismiss")
 * @param user_data User data passed to callback
 */
typedef void (*BreakOverlayCallback)(const char *action, gpointer user_data);

/**
 * Creates a new break overlay
 * @return New BreakOverlay instance
 */
BreakOverlay* break_overlay_new(void);

/**
 * Frees a break overlay
 * @param overlay BreakOverlay instance to free
 */
void break_overlay_free(BreakOverlay *overlay);

/**
 * Sets the callback for overlay actions
 * @param overlay BreakOverlay instance
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void break_overlay_set_callback(BreakOverlay *overlay, BreakOverlayCallback callback, gpointer user_data);

/**
 * Shows the break overlay
 * @param overlay BreakOverlay instance
 * @param break_type Break type ("Short Break" or "Long Break")
 * @param minutes Remaining minutes
 * @param seconds Remaining seconds
 */
void break_overlay_show(BreakOverlay *overlay, const char *break_type, int minutes, int seconds);

/**
 * Hides the break overlay
 * @param overlay BreakOverlay instance
 */
void break_overlay_hide(BreakOverlay *overlay);

/**
 * Updates the countdown display
 * @param overlay BreakOverlay instance
 * @param minutes Remaining minutes
 * @param seconds Remaining seconds
 */
void break_overlay_update_time(BreakOverlay *overlay, int minutes, int seconds);

/**
 * Updates the break type and motivational message
 * @param overlay BreakOverlay instance
 * @param break_type Break type ("Short Break" or "Long Break" or "Paused")
 */
void break_overlay_update_type(BreakOverlay *overlay, const char *break_type);

/**
 * Sets whether the overlay is visible
 * @param overlay BreakOverlay instance
 * @return TRUE if visible, FALSE otherwise
 */
gboolean break_overlay_is_visible(BreakOverlay *overlay);

G_END_DECLS

#endif // BREAK_OVERLAY_H