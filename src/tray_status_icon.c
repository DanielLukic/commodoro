#include "tray_status_icon.h"

// GTK3-only includes - this file compiles separately with GTK3 flags
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

struct _TrayStatusIcon {
    GtkStatusIcon *status_icon;
    TrayStatusIconCallback callback;
    gpointer user_data;
};

static void on_tray_activate(GtkStatusIcon *status_icon, TrayStatusIcon *tray);
static void on_tray_popup_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, TrayStatusIcon *tray);

TrayStatusIcon* tray_status_icon_new(void) {
    TrayStatusIcon *tray = g_malloc0(sizeof(TrayStatusIcon));
    
    // Create GTK3 status icon
    tray->status_icon = gtk_status_icon_new();
    
    // Set default properties
    gtk_status_icon_set_title(tray->status_icon, "Commodoro");
    gtk_status_icon_set_tooltip_text(tray->status_icon, "Commodoro Timer");
    gtk_status_icon_set_visible(tray->status_icon, TRUE);
    
    // Connect signals
    g_signal_connect(tray->status_icon, "activate", G_CALLBACK(on_tray_activate), tray);
    g_signal_connect(tray->status_icon, "popup-menu", G_CALLBACK(on_tray_popup_menu), tray);
    
    return tray;
}

void tray_status_icon_free(TrayStatusIcon *tray) {
    if (!tray) return;
    
    if (tray->status_icon) {
        g_object_unref(tray->status_icon);
    }
    
    g_free(tray);
}

void tray_status_icon_set_callback(TrayStatusIcon *tray, TrayStatusIconCallback callback, gpointer user_data) {
    if (!tray) return;
    
    tray->callback = callback;
    tray->user_data = user_data;
}

void tray_status_icon_update(TrayStatusIcon *tray, cairo_surface_t *surface, const char *tooltip) {
    if (!tray || !surface) return;
    
    // Convert Cairo surface to GdkPixbuf
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    
    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);
    if (pixbuf) {
        // Update icon
        gtk_status_icon_set_from_pixbuf(tray->status_icon, pixbuf);
        g_object_unref(pixbuf);
    }
    
    // Update tooltip
    if (tooltip) {
        gtk_status_icon_set_tooltip_text(tray->status_icon, tooltip);
    }
}

void tray_status_icon_set_visible(TrayStatusIcon *tray, gboolean visible) {
    if (!tray) return;
    
    gtk_status_icon_set_visible(tray->status_icon, visible);
}

gboolean tray_status_icon_is_embedded(TrayStatusIcon *tray) {
    if (!tray) return FALSE;
    
    return gtk_status_icon_is_embedded(tray->status_icon);
}

static void on_tray_activate(GtkStatusIcon *status_icon, TrayStatusIcon *tray) {
    (void)status_icon; // Suppress unused parameter warning
    
    if (tray->callback) {
        tray->callback("activate", tray->user_data);
    }
}

static void on_tray_popup_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, TrayStatusIcon *tray) {
    (void)status_icon; // Suppress unused parameter warning
    (void)button;      // Suppress unused parameter warning
    (void)activate_time; // Suppress unused parameter warning
    
    if (tray->callback) {
        tray->callback("popup-menu", tray->user_data);
    }
}