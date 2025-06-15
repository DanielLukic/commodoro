#include "system_tray.h"

// Only include GTK3 for AppIndicator - no GTK4 headers here
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>

struct _SystemTray {
    AppIndicator *indicator;
    GtkWidget *menu;
    GtkWidget *show_item;
    GtkWidget *separator1;
    GtkWidget *start_item;
    GtkWidget *pause_item;
    GtkWidget *reset_item;
    GtkWidget *separator2;
    GtkWidget *quit_item;
    
    SystemTrayCallback callback;
    gpointer user_data;
    
    char *temp_icon_path;
};

static void on_show_clicked(GtkMenuItem *item, SystemTray *tray);
static void on_start_clicked(GtkMenuItem *item, SystemTray *tray);
static void on_pause_clicked(GtkMenuItem *item, SystemTray *tray);
static void on_reset_clicked(GtkMenuItem *item, SystemTray *tray);
static void on_quit_clicked(GtkMenuItem *item, SystemTray *tray);
static char* create_temp_icon_file(cairo_surface_t *surface);

SystemTray* system_tray_new(void) {
    SystemTray *tray = g_malloc0(sizeof(SystemTray));
    
    // Create temporary icon file (will be updated later)
    tray->temp_icon_path = g_strdup("/tmp/commodoro-icon.png");
    
    // Create the AppIndicator
    tray->indicator = app_indicator_new("commodoro",
                                       "application-default-icon",
                                       APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    
    // Create context menu
    tray->menu = gtk_menu_new();
    
    // Show/Hide main window
    tray->show_item = gtk_menu_item_new_with_label("Show Commodoro");
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), tray->show_item);
    g_signal_connect(tray->show_item, "activate", G_CALLBACK(on_show_clicked), tray);
    
    // Separator
    tray->separator1 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), tray->separator1);
    
    // Timer controls
    tray->start_item = gtk_menu_item_new_with_label("Start");
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), tray->start_item);
    g_signal_connect(tray->start_item, "activate", G_CALLBACK(on_start_clicked), tray);
    
    tray->pause_item = gtk_menu_item_new_with_label("Pause");
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), tray->pause_item);
    g_signal_connect(tray->pause_item, "activate", G_CALLBACK(on_pause_clicked), tray);
    
    tray->reset_item = gtk_menu_item_new_with_label("Reset");
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), tray->reset_item);
    g_signal_connect(tray->reset_item, "activate", G_CALLBACK(on_reset_clicked), tray);
    
    // Separator
    tray->separator2 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), tray->separator2);
    
    // Quit
    tray->quit_item = gtk_menu_item_new_with_label("Quit");
    gtk_menu_shell_append(GTK_MENU_SHELL(tray->menu), tray->quit_item);
    g_signal_connect(tray->quit_item, "activate", G_CALLBACK(on_quit_clicked), tray);
    
    // Show all menu items
    gtk_widget_show_all(tray->menu);
    
    // Set menu on indicator
    app_indicator_set_menu(tray->indicator, GTK_MENU(tray->menu));
    
    // Set status to active
    app_indicator_set_status(tray->indicator, APP_INDICATOR_STATUS_ACTIVE);
    
    return tray;
}

void system_tray_free(SystemTray *tray) {
    if (!tray) return;
    
    if (tray->indicator) {
        g_object_unref(tray->indicator);
    }
    
    if (tray->temp_icon_path) {
        g_unlink(tray->temp_icon_path);  // Remove temp file
        g_free(tray->temp_icon_path);
    }
    
    g_free(tray);
}

void system_tray_set_callback(SystemTray *tray, SystemTrayCallback callback, gpointer user_data) {
    if (!tray) return;
    
    tray->callback = callback;
    tray->user_data = user_data;
}

void system_tray_update(SystemTray *tray, cairo_surface_t *surface, const char *tooltip) {
    if (!tray || !surface) return;
    
    // Save icon to temporary file
    char *icon_path = create_temp_icon_file(surface);
    if (icon_path) {
        app_indicator_set_icon_full(tray->indicator, icon_path, tooltip ? tooltip : "Commodoro");
        
        // Update stored path
        if (tray->temp_icon_path) {
            g_unlink(tray->temp_icon_path);
            g_free(tray->temp_icon_path);
        }
        tray->temp_icon_path = icon_path;
    }
}

void system_tray_update_menu(SystemTray *tray, gboolean can_start, gboolean can_pause, gboolean can_reset) {
    if (!tray) return;
    
    gtk_widget_set_sensitive(tray->start_item, can_start);
    gtk_widget_set_sensitive(tray->pause_item, can_pause);
    gtk_widget_set_sensitive(tray->reset_item, can_reset);
}

static void on_show_clicked(GtkMenuItem *item, SystemTray *tray) {
    (void)item; // Suppress unused parameter warning
    
    if (tray->callback) {
        tray->callback("show", tray->user_data);
    }
}

static void on_start_clicked(GtkMenuItem *item, SystemTray *tray) {
    (void)item; // Suppress unused parameter warning
    
    if (tray->callback) {
        tray->callback("start", tray->user_data);
    }
}

static void on_pause_clicked(GtkMenuItem *item, SystemTray *tray) {
    (void)item; // Suppress unused parameter warning
    
    if (tray->callback) {
        tray->callback("pause", tray->user_data);
    }
}

static void on_reset_clicked(GtkMenuItem *item, SystemTray *tray) {
    (void)item; // Suppress unused parameter warning
    
    if (tray->callback) {
        tray->callback("reset", tray->user_data);
    }
}

static void on_quit_clicked(GtkMenuItem *item, SystemTray *tray) {
    (void)item; // Suppress unused parameter warning
    
    if (tray->callback) {
        tray->callback("quit", tray->user_data);
    }
}

static char* create_temp_icon_file(cairo_surface_t *surface) {
    if (!surface) return NULL;
    
    // Create unique temporary filename
    char *temp_path = g_strdup_printf("/tmp/commodoro-icon-%d.png", g_random_int());
    
    // Save surface as PNG
    cairo_status_t status = cairo_surface_write_to_png(surface, temp_path);
    if (status != CAIRO_STATUS_SUCCESS) {
        g_free(temp_path);
        return NULL;
    }
    
    return temp_path;
}