// GTK workaround for Zig
// This file provides C wrappers to avoid the GdkEvent opaque type issue

#include <gtk/gtk.h>

// Initialize GTK without exposing argc/argv complications
void gtk_init_simple() {
    gtk_init(NULL, NULL);
}

// Window delete event handler that doesn't expose GdkEvent
gboolean window_delete_handler(GtkWidget *widget, GdkEvent *event, gpointer data) {
    // Just hide the window instead of destroying it
    gtk_widget_hide(widget);
    return TRUE; // Prevent default handler
}