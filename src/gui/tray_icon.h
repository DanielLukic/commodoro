#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include <gtk/gtk.h>
#include <cairo.h>

typedef struct _TrayIcon TrayIcon;

typedef enum {
    TRAY_STATE_IDLE,
    TRAY_STATE_WORK,
    TRAY_STATE_SHORT_BREAK,
    TRAY_STATE_LONG_BREAK,
    TRAY_STATE_PAUSED
} TrayIconState;

TrayIcon* tray_icon_new(void);
void tray_icon_free(TrayIcon *icon);
void tray_icon_update(TrayIcon *icon, TrayIconState state, int current_seconds, int total_seconds);
void tray_icon_set_tooltip(TrayIcon *icon, const char *tooltip);
cairo_surface_t* tray_icon_get_surface(TrayIcon *icon);

#endif // TRAY_ICON_H