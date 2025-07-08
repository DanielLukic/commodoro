#include "break_overlay.h"
#include <string.h>

struct _BreakOverlay {
    GtkWidget *window;
    GtkWidget *main_box;
    GtkWidget *title_label;
    GtkWidget *time_label;
    GtkWidget *message_label;
    GtkWidget *button_box;
    GtkWidget *skip_button;
    GtkWidget *extend_button;
    GtkWidget *pause_button;
    GtkWidget *dismiss_label;
    
    // Multi-monitor support
    GList *secondary_windows;  // List of secondary overlay windows
    
    BreakOverlayCallback callback;
    gpointer user_data;
};

static void on_skip_clicked(GtkButton *button, BreakOverlay *overlay);
static void on_extend_clicked(GtkButton *button, BreakOverlay *overlay);
static void on_pause_clicked(GtkButton *button, BreakOverlay *overlay);
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, BreakOverlay *overlay);
static const char* get_motivational_message(const char *break_type);
static void create_secondary_overlays(BreakOverlay *overlay);
static void destroy_secondary_overlays(BreakOverlay *overlay);

BreakOverlay* break_overlay_new(void) {
    BreakOverlay *overlay = g_malloc0(sizeof(BreakOverlay));
    
    // Initialize secondary windows list
    overlay->secondary_windows = NULL;
    
    // Create fullscreen window
    overlay->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(overlay->window), "Break Time");
    gtk_window_set_decorated(GTK_WINDOW(overlay->window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(overlay->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(overlay->window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(overlay->window), TRUE);
    gtk_window_stick(GTK_WINDOW(overlay->window));  // Show on all workspaces/desktops
    // Don't fullscreen here - we'll do it in show() with proper monitor assignment
    
    // Set black background
    GtkStyleContext *context = gtk_widget_get_style_context(overlay->window);
    gtk_style_context_add_class(context, "break-overlay");
    
    // Create main container
    overlay->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 40);
    gtk_widget_set_halign(overlay->main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(overlay->main_box, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(overlay->window), overlay->main_box);
    
    // Break type title
    overlay->title_label = gtk_label_new("Short Break");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->title_label), "break-title");
    gtk_widget_set_halign(overlay->title_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(overlay->main_box), overlay->title_label, FALSE, FALSE, 0);
    
    // Countdown timer
    overlay->time_label = gtk_label_new("04:04");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->time_label), "break-timer");
    gtk_widget_set_halign(overlay->time_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(overlay->main_box), overlay->time_label, FALSE, FALSE, 0);
    
    // Motivational message
    overlay->message_label = gtk_label_new("Take a quick breather");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->message_label), "break-message");
    gtk_widget_set_halign(overlay->message_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(overlay->main_box), overlay->message_label, FALSE, FALSE, 0);
    
    // Button container
    overlay->button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(overlay->button_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(overlay->main_box), overlay->button_box, FALSE, FALSE, 0);
    
    // Skip Break button
    overlay->skip_button = gtk_button_new_with_label("Skip Break (S)");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->skip_button), "break-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->skip_button), "break-button-destructive");
    gtk_widget_set_size_request(overlay->skip_button, 140, 40);
    gtk_box_pack_start(GTK_BOX(overlay->button_box), overlay->skip_button, FALSE, FALSE, 0);
    
    // Extend Break button
    overlay->extend_button = gtk_button_new_with_label("Extend Break (E)");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->extend_button), "break-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->extend_button), "break-button-normal");
    gtk_widget_set_size_request(overlay->extend_button, 140, 40);
    gtk_box_pack_start(GTK_BOX(overlay->button_box), overlay->extend_button, FALSE, FALSE, 0);
    
    // Pause button
    overlay->pause_button = gtk_button_new_with_label("Pause (P)");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->pause_button), "break-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->pause_button), "break-button-warning");
    gtk_widget_set_size_request(overlay->pause_button, 140, 40);
    gtk_box_pack_start(GTK_BOX(overlay->button_box), overlay->pause_button, FALSE, FALSE, 0);
    
    // ESC to dismiss label
    overlay->dismiss_label = gtk_label_new("Press ESC to dismiss");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay->dismiss_label), "break-dismiss");
    gtk_widget_set_halign(overlay->dismiss_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(overlay->dismiss_label, 40);
    gtk_box_pack_start(GTK_BOX(overlay->main_box), overlay->dismiss_label, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(overlay->skip_button, "clicked", G_CALLBACK(on_skip_clicked), overlay);
    g_signal_connect(overlay->extend_button, "clicked", G_CALLBACK(on_extend_clicked), overlay);
    g_signal_connect(overlay->pause_button, "clicked", G_CALLBACK(on_pause_clicked), overlay);
    g_signal_connect(overlay->window, "key-press-event", G_CALLBACK(on_key_press), overlay);
    
    // Load CSS for styling
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
        ".break-overlay { background-color: #000000; }"
        ".break-title { font-size: 48px; color: #4dd0e1; font-weight: bold; }"
        ".break-timer { font-size: 96px; color: #ffffff; font-weight: bold; font-family: monospace; }"
        ".break-message { font-size: 24px; color: #888888; }"
        ".break-button { "
        "  min-width: 120px; min-height: 40px; "
        "  font-size: 16px; font-weight: bold; "
        "  border-radius: 8px; "
        "}"
        ".break-button-destructive { "
        "  background-color: #d32f2f; color: #ffffff; "
        "  border: 2px solid #b71c1c; "
        "}"
        ".break-button-destructive:hover { background-color: #f44336; }"
        ".break-button-normal { "
        "  background-color: #1976d2; color: #ffffff; "
        "  border: 2px solid #0d47a1; "
        "}"
        ".break-button-normal:hover { background-color: #2196f3; }"
        ".break-button-warning { "
        "  background-color: #f57c00; color: #ffffff; "
        "  border: 2px solid #e65100; "
        "}"
        ".break-button-warning:hover { background-color: #ff9800; }"
        ".break-dismiss { font-size: 16px; color: #666666; }",
        -1, NULL);
    
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(css_provider);
    
    // Initially hidden
    gtk_widget_hide(overlay->window);
    
    return overlay;
}

void break_overlay_free(BreakOverlay *overlay) {
    if (!overlay) return;
    
    destroy_secondary_overlays(overlay);
    gtk_widget_destroy(overlay->window);
    g_free(overlay);
}

void break_overlay_set_callback(BreakOverlay *overlay, BreakOverlayCallback callback, gpointer user_data) {
    if (!overlay) return;
    
    overlay->callback = callback;
    overlay->user_data = user_data;
}

void break_overlay_show(BreakOverlay *overlay, const char *break_type, int minutes, int seconds) {
    if (!overlay) return;
    
    // Update content
    break_overlay_update_type(overlay, break_type);
    break_overlay_update_time(overlay, minutes, seconds);
    
    // Get display and monitor info
    GdkDisplay *display = gdk_display_get_default();
    int n_monitors = gdk_display_get_n_monitors(display);
    
    if (n_monitors > 0) {
        // Get first monitor geometry
        GdkMonitor *monitor = gdk_display_get_monitor(display, 0);
        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);
        
        // Position main window at exact monitor coordinates
        gtk_window_move(GTK_WINDOW(overlay->window), geometry.x, geometry.y);
        gtk_window_resize(GTK_WINDOW(overlay->window), geometry.width, geometry.height);
        gtk_window_set_decorated(GTK_WINDOW(overlay->window), FALSE);
        
        // Show and then fullscreen
        gtk_widget_show_all(overlay->window);
        gtk_window_fullscreen(GTK_WINDOW(overlay->window));
        gtk_window_present(GTK_WINDOW(overlay->window));
    }
    
    // Create secondary overlays for other monitors
    if (n_monitors > 1) {
        create_secondary_overlays(overlay);
    }
    
    // Grab focus to ensure key events are captured
    gtk_widget_grab_focus(overlay->window);
}

void break_overlay_hide(BreakOverlay *overlay) {
    if (!overlay) return;
    
    gtk_widget_hide(overlay->window);
    
    // Hide and destroy secondary windows
    destroy_secondary_overlays(overlay);
}

void break_overlay_update_time(BreakOverlay *overlay, int minutes, int seconds) {
    if (!overlay) return;
    
    char time_text[16];
    g_snprintf(time_text, sizeof(time_text), "%02d:%02d", minutes, seconds);
    gtk_label_set_text(GTK_LABEL(overlay->time_label), time_text);
}

void break_overlay_update_type(BreakOverlay *overlay, const char *break_type) {
    if (!overlay || !break_type) return;
    
    gtk_label_set_text(GTK_LABEL(overlay->title_label), break_type);
    
    const char *message = get_motivational_message(break_type);
    gtk_label_set_text(GTK_LABEL(overlay->message_label), message);
    
    // Update title color based on break type
    GtkStyleContext *context = gtk_widget_get_style_context(overlay->title_label);
    gtk_style_context_remove_class(context, "break-title-short");
    gtk_style_context_remove_class(context, "break-title-long");
    gtk_style_context_remove_class(context, "break-title-paused");
    
    if (g_strcmp0(break_type, "Short Break") == 0) {
        gtk_style_context_add_class(context, "break-title-short");
    } else if (g_strcmp0(break_type, "Long Break") == 0) {
        gtk_style_context_add_class(context, "break-title-long");
    } else if (g_strcmp0(break_type, "Paused") == 0) {
        gtk_style_context_add_class(context, "break-title-paused");
    }
}

gboolean break_overlay_is_visible(BreakOverlay *overlay) {
    if (!overlay) return FALSE;
    
    return gtk_widget_get_visible(overlay->window);
}

void break_overlay_update_pause_button(BreakOverlay *overlay, const char *label) {
    if (!overlay || !label) return;
    
    // Add keyboard shortcut to the label
    char label_with_shortcut[32];
    if (g_strcmp0(label, "Pause") == 0) {
        g_snprintf(label_with_shortcut, sizeof(label_with_shortcut), "Pause (P)");
    } else if (g_strcmp0(label, "Resume") == 0) {
        g_snprintf(label_with_shortcut, sizeof(label_with_shortcut), "Resume (R)");
    } else {
        g_snprintf(label_with_shortcut, sizeof(label_with_shortcut), "%s", label);
    }
    
    gtk_button_set_label(GTK_BUTTON(overlay->pause_button), label_with_shortcut);
}

// Event handlers
static void on_skip_clicked(GtkButton *button, BreakOverlay *overlay) {
    (void)button; // Suppress unused parameter warning
    
    if (overlay->callback) {
        overlay->callback("skip_break", overlay->user_data);
    }
}

static void on_extend_clicked(GtkButton *button, BreakOverlay *overlay) {
    (void)button; // Suppress unused parameter warning
    
    if (overlay->callback) {
        overlay->callback("extend_break", overlay->user_data);
    }
}

static void on_pause_clicked(GtkButton *button, BreakOverlay *overlay) {
    (void)button; // Suppress unused parameter warning
    
    if (overlay->callback) {
        overlay->callback("pause", overlay->user_data);
    }
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, BreakOverlay *overlay) {
    (void)widget; // Suppress unused parameter warning
    
    if (event->keyval == GDK_KEY_Escape) {
        if (overlay->callback) {
            overlay->callback("dismiss", overlay->user_data);
        }
        return TRUE;
    }
    
    // Handle keyboard shortcuts for buttons
    switch (event->keyval) {
        case GDK_KEY_s:
        case GDK_KEY_S:
            // Skip break
            if (overlay->callback) {
                overlay->callback("skip_break", overlay->user_data);
            }
            return TRUE;
            
        case GDK_KEY_e:
        case GDK_KEY_E:
            // Extend break
            if (overlay->callback) {
                overlay->callback("extend_break", overlay->user_data);
            }
            return TRUE;
            
        case GDK_KEY_p:
        case GDK_KEY_P:
            // Pause break
            if (overlay->callback) {
                overlay->callback("pause", overlay->user_data);
            }
            return TRUE;
            
        case GDK_KEY_r:
        case GDK_KEY_R:
            // Resume break (same as pause - toggles)
            if (overlay->callback) {
                overlay->callback("pause", overlay->user_data);
            }
            return TRUE;
            
        default:
            break;
    }
    
    return FALSE;
}

static const char* get_motivational_message(const char *break_type) {
    if (g_strcmp0(break_type, "Short Break") == 0) {
        return "Take a quick breather";
    } else if (g_strcmp0(break_type, "Long Break") == 0) {
        return "Time for a longer rest";
    } else if (g_strcmp0(break_type, "Paused") == 0) {
        return "Break timer paused";
    } else {
        return "Take a moment to relax";
    }
}

static void create_secondary_overlays(BreakOverlay *overlay) {
    if (!overlay) return;
    
    // Clean up any existing secondary windows first
    destroy_secondary_overlays(overlay);
    
    // Get the default display and number of monitors
    GdkDisplay *display = gdk_display_get_default();
    int n_monitors = gdk_display_get_n_monitors(display);
    
    // Create overlay windows for all monitors except monitor 0 (where main overlay is)
    for (int i = 1; i < n_monitors; i++) {
        // Get monitor geometry
        GdkMonitor *monitor = gdk_display_get_monitor(display, i);
        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);
        
        // Create a simple overlay window
        GtkWidget *secondary_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(secondary_window), FALSE);
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(secondary_window), TRUE);
        gtk_window_set_skip_pager_hint(GTK_WINDOW(secondary_window), TRUE);
        gtk_window_set_keep_above(GTK_WINDOW(secondary_window), TRUE);
        gtk_window_stick(GTK_WINDOW(secondary_window));
        
        // Position window at exact monitor coordinates BEFORE showing
        gtk_window_move(GTK_WINDOW(secondary_window), geometry.x, geometry.y);
        gtk_window_resize(GTK_WINDOW(secondary_window), geometry.width, geometry.height);
        
        // Set black background
        GtkStyleContext *context = gtk_widget_get_style_context(secondary_window);
        gtk_style_context_add_class(context, "break-overlay");
        
        // Connect key press event to allow ESC on any monitor
        g_signal_connect(secondary_window, "key-press-event", G_CALLBACK(on_key_press), overlay);
        
        // Show and fullscreen
        gtk_widget_show_all(secondary_window);
        gtk_window_fullscreen(GTK_WINDOW(secondary_window));
        gtk_window_present(GTK_WINDOW(secondary_window));
        
        // Add to list
        overlay->secondary_windows = g_list_append(overlay->secondary_windows, secondary_window);
    }
}

static void destroy_secondary_overlays(BreakOverlay *overlay) {
    if (!overlay || !overlay->secondary_windows) return;
    
    // Destroy all secondary windows
    GList *iter;
    for (iter = overlay->secondary_windows; iter != NULL; iter = iter->next) {
        GtkWidget *window = GTK_WIDGET(iter->data);
        gtk_widget_destroy(window);
    }
    
    // Clear the list
    g_list_free(overlay->secondary_windows);
    overlay->secondary_windows = NULL;
}

