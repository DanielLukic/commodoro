#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tray_icon_rust.h"
#include "timer_rust.h"
#include "tray_status_icon.h"
#include "audio_rust.h"
#include "settings_dialog.h"
#include "break_overlay.h"
#include "config_rust.h"
#include "app.h"
#include "callbacks.h"
#include "dbus.h"


static void on_settings_clicked(GtkButton *button, GomodaroApp *app);
static void on_timer_state_changed(Timer *timer, TimerState state, gpointer user_data);
static void on_timer_tick(Timer *timer, int minutes, int seconds, gpointer user_data);
static void on_timer_session_complete(Timer *timer, TimerState completed_state, gpointer user_data);
static void update_display(GomodaroApp *app);
static gboolean on_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
static void on_tray_status_action(const char *action, gpointer user_data);
static void on_settings_dialog_action(const char *action, gpointer user_data);
static void apply_settings(GomodaroApp *app);
static void on_break_overlay_action(const char *action, gpointer user_data);
static void on_auto_start_toggled(GtkToggleButton *button, gpointer user_data);
static void cleanup_app(GomodaroApp *app);
static gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static void quit_application(GomodaroApp *app);
static void on_tray_auto_start_toggled(GtkCheckMenuItem *item, gpointer user_data);
static void on_tray_config_clicked(GtkMenuItem *item, gpointer user_data);
static void on_tray_start_clicked(GtkMenuItem *item, gpointer user_data);
static void on_tray_reset_clicked(GtkMenuItem *item, gpointer user_data);
static gboolean on_input_activity_detected(gpointer user_data);
static gboolean delayed_window_present(gpointer user_data);
static gboolean check_idle_timeout(gpointer user_data);
static void start_idle_monitoring(GomodaroApp *app);
static void stop_idle_monitoring(GomodaroApp *app);

// Command line argument parsing
static int parse_duration_to_seconds(const char *duration_str) {
    if (!duration_str) return 0;
    
    int len = strlen(duration_str);
    if (len == 0) return 0;
    
    char *endptr;
    int value = strtol(duration_str, &endptr, 10);
    
    if (endptr == duration_str) {
        // No digits found
        return 0;
    }
    
    // Check for time unit suffix
    if (*endptr == 's') {
        // Seconds
        return value;
    } else if (*endptr == 'm') {
        // Minutes
        return value * 60;
    } else if (*endptr == 'h') {
        // Hours
        return value * 3600;
    } else if (*endptr == '\0') {
        // No suffix, assume minutes
        return value * 60;
    }
    
    return 0; // Invalid format
}

static int seconds_to_duration_units(int seconds) {
    // Return duration in the units expected by timer (minutes for normal, seconds for test)
    return seconds; // Store actual seconds, timer will handle conversion
}

static CmdLineArgs* parse_command_line(int argc, char *argv[]) {
    CmdLineArgs *args = g_malloc0(sizeof(CmdLineArgs));
    
    // Set defaults (25/5/15 minute Pomodoro, 4 sessions)
    args->work_duration = 25;
    args->short_break_duration = 5;
    args->long_break_duration = 15;
    args->sessions_until_long_break = 4;
    args->test_mode = FALSE;
    
    // Parse positional arguments: work_duration short_break_duration sessions_until_long long_break_duration
    if (argc >= 2) {
        int work_seconds = parse_duration_to_seconds(argv[1]);
        if (work_seconds > 0) {
            args->work_duration = seconds_to_duration_units(work_seconds);
            args->test_mode = TRUE;
            g_print("🧪 TEST MODE ACTIVE\n");
            g_print("Work: %s (%d seconds)\n", argv[1], args->work_duration);
        }
    }
    
    if (argc >= 3) {
        int short_break_seconds = parse_duration_to_seconds(argv[2]);
        if (short_break_seconds > 0) {
            args->short_break_duration = seconds_to_duration_units(short_break_seconds);
            g_print("Short break: %s (%d seconds)\n", argv[2], args->short_break_duration);
        }
    }
    
    if (argc >= 4) {
        int sessions = atoi(argv[3]);
        if (sessions > 0) {
            args->sessions_until_long_break = sessions;
            g_print("Sessions until long break: %d\n", args->sessions_until_long_break);
        }
    }
    
    if (argc >= 5) {
        int long_break_seconds = parse_duration_to_seconds(argv[4]);
        if (long_break_seconds > 0) {
            args->long_break_duration = seconds_to_duration_units(long_break_seconds);
            g_print("Long break: %s (%d seconds)\n", argv[4], args->long_break_duration);
        }
    }
    
    if (args->test_mode) {
        g_print("\n");
    } else {
        g_print("Normal mode - using default durations (25m/5m/15m, 4 sessions)\n");
    }
    
    return args;
}

static void print_usage(const char *program_name) {
    g_print("Usage: %s [work_duration] [short_break_duration] [sessions_until_long] [long_break_duration]\n", program_name);
    g_print("       %s <command> [--auto-start]\n\n", program_name);
    g_print("Timer mode examples:\n");
    g_print("  %s                    # Normal mode (25m work, 5m break)\n", program_name);
    g_print("  %s 15s 5s 4 10s       # Test mode (15s work, 5s break, 4 cycles, 10s long break)\n", program_name);
    g_print("  %s 2m 30s 2 1m        # Quick test (2m work, 30s break, 2 cycles, 1m long break)\n", program_name);
    g_print("  %s 45m 10m 3 20m      # Extended mode (45m work, 10m break, 3 cycles, 20m long break)\n\n", program_name);
    g_print("Time units:\n");
    g_print("  s = seconds, m = minutes, h = hours\n");
    g_print("  No suffix defaults to minutes\n\n");
    g_print("D-Bus commands:\n");
    g_print("  toggle_timer          # Start/pause/resume the timer\n");
    g_print("  reset_timer           # Reset the timer\n");
    g_print("  toggle_break          # Skip to next phase\n");
    g_print("  show_hide             # Toggle window visibility\n");
    g_print("  --auto-start          # Start Commodoro if not running\n\n");
}

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    (void)gtk_app; // Suppress unused parameter warning
    CmdLineArgs *cmd_args = (CmdLineArgs *)user_data;
    GomodaroApp *app = g_new0(GomodaroApp, 1);
    
    // Store command line args
    app->args = cmd_args;
    
    // Create config manager (in-memory for test mode, persistent for normal mode)
    gboolean use_persistent = !(cmd_args && cmd_args->test_mode);
    app->config = config_new(use_persistent);
    app->settings = config_load_settings(app->config);
    
    // Override settings with command line arguments if in test mode
    if (cmd_args && cmd_args->test_mode) {
        app->settings->work_duration = cmd_args->work_duration;
        app->settings->short_break_duration = cmd_args->short_break_duration;
        app->settings->long_break_duration = cmd_args->long_break_duration;
        app->settings->sessions_until_long_break = cmd_args->sessions_until_long_break;
    }
    
    // Create audio manager
    app->audio = audio_manager_new();
    
    // Create timer with default durations
    app->timer = timer_new();
    timer_set_durations(app->timer, app->settings->work_duration, app->settings->short_break_duration, 
                       app->settings->long_break_duration, app->settings->sessions_until_long_break);
    timer_set_callbacks(app->timer, on_timer_state_changed, on_timer_tick, on_timer_session_complete, app);
    
    // Set timer mode based on test mode
    if (cmd_args && cmd_args->test_mode) {
        timer_set_duration_mode(app->timer, TRUE);  // Use seconds mode for test
    }
    
    // Apply initial settings
    apply_settings(app);
    
    // Create tray icon
    app->tray_icon = tray_icon_new();
    tray_icon_set_tooltip(app->tray_icon, "Commodoro - Ready to start");
    
    // Create status tray
    app->status_tray = tray_status_icon_new();
    tray_status_icon_set_callback(app->status_tray, on_tray_status_action, app);
    
    // Create break overlay
    app->break_overlay = break_overlay_new();
    break_overlay_set_callback(app->break_overlay, on_break_overlay_action, app);
    
    // Create input monitor for auto-start detection
    app->input_monitor = input_monitor_new();
    input_monitor_set_callback(app->input_monitor, on_input_activity_detected, app);

    // Create and publish D-Bus service
    app->dbus_service = dbus_service_new(app);
    dbus_service_publish(app->dbus_service);
    
    // Create main window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "Commodoro");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 400, 500);
    gtk_window_set_resizable(GTK_WINDOW(app->window), FALSE);
    
    // Set window to appear centered and get focus
    gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_CENTER);
    
    // Enable event masks for input detection (key presses, button presses, motion)
    gtk_widget_add_events(app->window, 
                         GDK_KEY_PRESS_MASK | 
                         GDK_BUTTON_PRESS_MASK | 
                         GDK_POINTER_MOTION_MASK);
    
    // Create main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(main_box, 30);
    gtk_widget_set_margin_bottom(main_box, 30);
    gtk_widget_set_margin_start(main_box, 30);
    gtk_widget_set_margin_end(main_box, 30);
    gtk_container_add(GTK_CONTAINER(app->window), main_box);
    
    // Time display
    app->time_label = gtk_label_new("25:00");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->time_label), "time-display");
    gtk_widget_set_halign(app->time_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), app->time_label, FALSE, FALSE, 0);
    
    // Status label
    app->status_label = gtk_label_new("Ready to start");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->status_label), "status-label");
    gtk_widget_set_halign(app->status_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), app->status_label, FALSE, FALSE, 0);
    
    // Button container
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 0);
    
    // Control buttons
    app->start_button = gtk_button_new_with_label("Start");
    app->pause_button = gtk_button_new_with_label("Pause");
    app->reset_button = gtk_button_new_with_label("Reset");
    app->settings_button = gtk_button_new_with_label("⚙");
    
    gtk_style_context_add_class(gtk_widget_get_style_context(app->start_button), "control-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->pause_button), "control-button");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->reset_button), "control-button");
    
    // Hide pause button - using single button logic
    gtk_widget_hide(app->pause_button);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->settings_button), "settings-button");
    
    gtk_box_pack_start(GTK_BOX(button_box), app->start_button, FALSE, FALSE, 0);
    // Pause button removed - using single button logic
    gtk_box_pack_start(GTK_BOX(button_box), app->reset_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), app->settings_button, FALSE, FALSE, 0);
    
    // Session counter
    app->session_label = gtk_label_new("Session: 1");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->session_label), "session-label");
    gtk_widget_set_halign(app->session_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(app->session_label, 20);
    gtk_box_pack_start(GTK_BOX(main_box), app->session_label, FALSE, FALSE, 0);
    
    // Settings section (simple, no frame)  
    app->auto_start_check = gtk_check_button_new_with_label("Auto-Start Work (when user activity detected)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->auto_start_check), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->auto_start_check), "setting-check");
    gtk_widget_set_halign(app->auto_start_check, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(app->auto_start_check, 20);
    gtk_box_pack_start(GTK_BOX(main_box), app->auto_start_check, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(app->start_button, "clicked", G_CALLBACK(on_start_clicked), app);
    
    g_signal_connect(app->reset_button, "clicked", G_CALLBACK(on_reset_clicked), app);
    g_signal_connect(app->settings_button, "clicked", G_CALLBACK(on_settings_clicked), app);
    g_signal_connect(app->auto_start_check, "toggled", G_CALLBACK(on_auto_start_toggled), app);
    
    // Add keyboard shortcuts
    g_signal_connect(app->window, "key-press-event", G_CALLBACK(on_key_pressed), app);
    
    // Connect delete-event signal to hide window instead of destroying it
    g_signal_connect(app->window, "delete-event", G_CALLBACK(on_window_delete_event), app);
    
    // Load CSS
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
        "window { background-color: #2b2b2b; color: #ffffff; }"
        ".time-display { font-size: 72px; font-weight: bold; color: #f4e4c1; }"
        ".status-label { font-size: 18px; color: #888888; margin-bottom: 20px; }"
        ".control-button { min-width: 80px; min-height: 40px; margin: 0 5px; "
        "  background-color: #404040; color: #ffffff; border: 1px solid #555555; }"
        ".control-button:hover { background-color: #505050; }"
        ".settings-button { min-width: 40px; min-height: 40px; margin: 0 5px; "
        "  background-color: #404040; color: #ffffff; border: 1px solid #555555; }"
        ".settings-button:hover { background-color: #505050; }"
        ".session-label { font-size: 16px; color: #ffffff; }"
        ".setting-check { color: #ffffff; }"
        ".setting-check check { background-color: #404040; border: 1px solid #555555; margin-right: 12px; }"
        ".setting-check check:checked { background-color: #4CAF50; }",
        -1, NULL);
    
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(css_provider);
    
    // Store app pointer in window data
    g_object_set_data(G_OBJECT(app->window), "app", app);
    
    // Assign window to input monitor for GTK event-based detection
    input_monitor_set_window(app->input_monitor, app->window);
    
    // Show all widgets and present window with enhanced focus (matches working pymodoro pattern)
    gtk_widget_show_all(app->window);
    gtk_window_present(GTK_WINDOW(app->window));
    
    // Additional focus and urgency hints to ensure window appears on top
    gtk_window_set_urgency_hint(GTK_WINDOW(app->window), TRUE);
    gtk_widget_grab_focus(app->window);
    
    // Try to ensure window gets focus after a brief delay to combat focus stealing
    g_timeout_add(100, delayed_window_present, app->window);
    
    // Initial display update
    update_display(app);
    
    // Check if we should execute a startup command
    const char *startup_cmd = g_getenv("COMMODORO_STARTUP_CMD");
    if (startup_cmd) {
        if (g_strcmp0(startup_cmd, "ToggleTimer") == 0) {
            // Start the timer
            timer_start(app->timer);
        }
        // Clear the environment variable
        g_unsetenv("COMMODORO_STARTUP_CMD");
    }
}

void on_start_clicked(GtkButton *button, GomodaroApp *app) {
    (void)button; // Suppress unused parameter warning
    
    TimerState state = timer_get_state(app->timer);
    
    // Single button logic: Start/Pause/Resume
    if (state == TIMER_STATE_IDLE || state == TIMER_STATE_PAUSED) {
        timer_start(app->timer);
    } else if (state == TIMER_STATE_WORK || state == TIMER_STATE_SHORT_BREAK || 
               state == TIMER_STATE_LONG_BREAK) {
        timer_pause(app->timer);
    }
}



void on_reset_clicked(GtkButton *button, GomodaroApp *app) {
    (void)button; // Suppress unused parameter warning
    timer_reset(app->timer);
}

static void on_settings_clicked(GtkButton *button, GomodaroApp *app) {
    (void)button; // Suppress unused parameter warning
    
    SettingsDialog *dialog = settings_dialog_new(GTK_WINDOW(app->window), app->settings, app->audio);
    settings_dialog_set_callback(dialog, on_settings_dialog_action, app);
    settings_dialog_show(dialog);
    
    // Store dialog pointer temporarily for cleanup
    g_object_set_data(G_OBJECT(app->window), "settings_dialog", dialog);
}

static void on_timer_state_changed(Timer *timer, TimerState state, gpointer user_data) {
    (void)timer; // Suppress unused parameter warning
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    switch (state) {
        case TIMER_STATE_IDLE:
            gtk_button_set_label(GTK_BUTTON(app->start_button), "Start");
            gtk_widget_set_sensitive(app->start_button, TRUE);
            gtk_widget_set_sensitive(app->reset_button, TRUE);
            // Hide break overlay when returning to idle
            break_overlay_hide(app->break_overlay);
            
            // Stop idle monitoring when idle
            stop_idle_monitoring(app);
            
            // Clear idle pause flag
            app->paused_by_idle = FALSE;
            
            // Start input monitoring if auto-start is enabled
            if (app->settings && app->settings->auto_start_work_after_break) {
                g_print("Timer transitioned to IDLE, starting input monitor for auto-start\n");
                input_monitor_start(app->input_monitor);
            } else {
                g_print("Timer transitioned to IDLE, but auto-start is disabled\n");
            }
            break;
            
        case TIMER_STATE_WORK:
            gtk_button_set_label(GTK_BUTTON(app->start_button), "Pause");
            gtk_widget_set_sensitive(app->start_button, TRUE);
            gtk_widget_set_sensitive(app->reset_button, TRUE);
            // Play work start sound
            audio_manager_play_work_start(app->audio);
            // Hide break overlay during work
            break_overlay_hide(app->break_overlay);
            // Stop input monitoring when work starts
            input_monitor_stop(app->input_monitor);
            // Start idle detection during work sessions
            start_idle_monitoring(app);
            break;
            
        case TIMER_STATE_SHORT_BREAK:
            gtk_button_set_label(GTK_BUTTON(app->start_button), "Pause");
            gtk_widget_set_sensitive(app->start_button, TRUE);
            gtk_widget_set_sensitive(app->reset_button, TRUE);
            // Play break start sound
            audio_manager_play_break_start(app->audio);
            // Show break overlay
            int minutes, seconds;
            timer_get_remaining(app->timer, &minutes, &seconds);
            break_overlay_show(app->break_overlay, "Short Break", minutes, seconds);
            // Stop idle monitoring during breaks
            stop_idle_monitoring(app);
            break;
            
        case TIMER_STATE_LONG_BREAK:
            gtk_button_set_label(GTK_BUTTON(app->start_button), "Pause");
            gtk_widget_set_sensitive(app->start_button, TRUE);
            gtk_widget_set_sensitive(app->reset_button, TRUE);
            // Play long break start sound
            audio_manager_play_long_break_start(app->audio);
            // Show break overlay
            timer_get_remaining(app->timer, &minutes, &seconds);
            break_overlay_show(app->break_overlay, "Long Break", minutes, seconds);
            // Stop idle monitoring during breaks
            stop_idle_monitoring(app);
            break;
            
        case TIMER_STATE_PAUSED:
            gtk_button_set_label(GTK_BUTTON(app->start_button), "Resume");
            gtk_widget_set_sensitive(app->start_button, TRUE);
            gtk_widget_set_sensitive(app->reset_button, TRUE);
            // Stop idle monitoring when paused (unless paused by idle)
            if (!app->paused_by_idle) {
                stop_idle_monitoring(app);
            }
            break;
    }
    
    update_display(app);
}

static void on_timer_tick(Timer *timer, int minutes, int seconds, gpointer user_data) {
    (void)timer; // Suppress unused parameter warning
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    update_display(app);
    
    // Update break overlay if visible
    if (break_overlay_is_visible(app->break_overlay)) {
        break_overlay_update_time(app->break_overlay, minutes, seconds);
    }
}

static void on_timer_session_complete(Timer *timer, TimerState completed_state, gpointer user_data) {
    (void)timer; // Suppress unused parameter warning
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    // Play appropriate sound based on what just completed
    switch (completed_state) {
        case TIMER_STATE_WORK:
            // Work session completed - play session complete sound
            audio_manager_play_session_complete(app->audio);
            break;
            
        case TIMER_STATE_SHORT_BREAK:
        case TIMER_STATE_LONG_BREAK:
            // Break completed - play timer finish sound (matches Python behavior)
            audio_manager_play_timer_finish(app->audio);
            break;
            
        default:
            // Other states don't trigger completion sounds
            break;
    }
}

static void update_display(GomodaroApp *app) {
    char time_text[16];
    char session_text[32];
    char tooltip_text[64];
    
    // Get current timer state
    TimerState state = timer_get_state(app->timer);
    int minutes, seconds;
    timer_get_remaining(app->timer, &minutes, &seconds);
    int session = timer_get_session(app->timer);
    
    // Update time display
    g_snprintf(time_text, sizeof(time_text), "%02d:%02d", minutes, seconds);
    gtk_label_set_text(GTK_LABEL(app->time_label), time_text);
    
    // Update session display
    g_snprintf(session_text, sizeof(session_text), "Session: %d", session);
    gtk_label_set_text(GTK_LABEL(app->session_label), session_text);
    
    // Update status display
    const char *status;
    switch (state) {
        case TIMER_STATE_IDLE:
            status = "Ready to start";
            break;
        case TIMER_STATE_WORK:
            status = "Work Session";
            break;
        case TIMER_STATE_SHORT_BREAK:
            status = "Short Break";
            break;
        case TIMER_STATE_LONG_BREAK:
            status = "Long Break";
            break;
        case TIMER_STATE_PAUSED:
            status = "Paused";
            break;
        default:
            status = "Unknown";
            break;
    }
    gtk_label_set_text(GTK_LABEL(app->status_label), status);
    
    // Update tray icon with state, time, and progress
    int total_seconds = timer_get_total_duration(app->timer);
    int current_seconds = minutes * 60 + seconds;
    
    tray_icon_update(app->tray_icon, state, current_seconds, total_seconds);
    
    g_snprintf(tooltip_text, sizeof(tooltip_text), "Commodoro - %s (%02d:%02d remaining)", 
               status, minutes, seconds);
    tray_icon_set_tooltip(app->tray_icon, tooltip_text);
    
    // Update status tray
    cairo_surface_t *surface = tray_icon_get_surface(app->tray_icon);
    if (surface) {
        tray_status_icon_update(app->status_tray, surface, tooltip_text);
    }
}

static gboolean on_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget; // Suppress unused parameter warning
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    // Check for Ctrl+Q - quit application
    if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_q) {
        quit_application(app);
        return TRUE;
    }
    
    // Check for Escape - hide window
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_hide(app->window);
        return TRUE;
    }
    
    return FALSE;
}

static void on_tray_status_action(const char *action, gpointer user_data) {
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    if (g_strcmp0(action, "activate") == 0) {
        // Toggle main window visibility on click
        if (gtk_widget_get_visible(app->window)) {
            gtk_widget_hide(app->window);
        } else {
            // Show window similar to PyQt's show() + raise_() + activateWindow()
            gtk_widget_show(app->window);
            gtk_window_present(GTK_WINDOW(app->window));
            gtk_window_set_urgency_hint(GTK_WINDOW(app->window), TRUE);
        }
    } else if (g_strcmp0(action, "popup-menu") == 0) {
        // Right-click - show context menu
        GtkWidget *menu = gtk_menu_new();
        
        // Timer control items
        TimerState state = timer_get_state(app->timer);
        
        // Start/Pause/Resume item
        const char *control_label;
        if (state == TIMER_STATE_IDLE) {
            control_label = "Start";
        } else if (state == TIMER_STATE_PAUSED) {
            control_label = "Resume";
        } else {
            control_label = "Pause";
        }
        
        GtkWidget *control_item = gtk_menu_item_new_with_label(control_label);
        g_signal_connect(control_item, "activate", 
                        G_CALLBACK(on_tray_start_clicked), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), control_item);
        
        // Reset item
        GtkWidget *reset_item = gtk_menu_item_new_with_label("Reset");
        gtk_widget_set_sensitive(reset_item, state != TIMER_STATE_IDLE);
        g_signal_connect(reset_item, "activate",
                        G_CALLBACK(on_tray_reset_clicked), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), reset_item);
        
        // Separator
        GtkWidget *separator1 = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator1);
        
        // Config menu item
        GtkWidget *config_item = gtk_menu_item_new_with_label("Config");
        g_signal_connect(config_item, "activate", 
                        G_CALLBACK(on_tray_config_clicked), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), config_item);
        
        // Separator
        GtkWidget *separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
        
        // Auto-Start toggle
        GtkWidget *auto_start_item = gtk_check_menu_item_new_with_label("Auto-Start Pomo");
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(auto_start_item), 
                                      app->settings->auto_start_work_after_break);
        g_signal_connect(auto_start_item, "toggled", 
                        G_CALLBACK(on_tray_auto_start_toggled), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), auto_start_item);
        
        // Separator
        GtkWidget *separator2 = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator2);
        
        // Quit menu item
        GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
        g_signal_connect_swapped(quit_item, "activate", 
                                G_CALLBACK(quit_application), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);
        
        // Show menu
        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
    }
}

static void on_settings_dialog_action(const char *action, gpointer user_data) {
    GomodaroApp *app = (GomodaroApp *)user_data;
    SettingsDialog *dialog = g_object_get_data(G_OBJECT(app->window), "settings_dialog");
    
    if (g_strcmp0(action, "ok") == 0) {
        // Apply new settings
        Settings *new_settings = settings_dialog_get_settings(dialog);
        settings_free(app->settings);
        app->settings = new_settings;
        
        // Save settings (config manager handles in-memory vs persistent)
        config_save_settings(app->config, app->settings);
        
        // Update timer with new durations
        timer_set_durations(app->timer, app->settings->work_duration, app->settings->short_break_duration,
                           app->settings->long_break_duration, app->settings->sessions_until_long_break);
        
        // Apply audio and other settings
        apply_settings(app);
        
        // Update display with new settings
        update_display(app);
    }
    
    // Clean up dialog
    settings_dialog_free(dialog);
    g_object_set_data(G_OBJECT(app->window), "settings_dialog", NULL);
}

static void apply_settings(GomodaroApp *app) {
    if (!app || !app->settings) return;
    
    // Apply audio settings
    audio_manager_set_enabled(app->audio, app->settings->enable_sounds);
    audio_manager_set_volume(app->audio, app->settings->sound_volume);
    
    // Apply timer settings
    timer_set_auto_start_work(app->timer, app->settings->auto_start_work_after_break);
    
    // Update auto-start checkbox to match settings
    if (app->auto_start_check && GTK_IS_TOGGLE_BUTTON(app->auto_start_check)) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->auto_start_check), 
                                     app->settings->auto_start_work_after_break);
    }
}

static void on_break_overlay_action(const char *action, gpointer user_data) {
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    if (g_strcmp0(action, "skip_break") == 0) {
        // Skip break and return to idle
        timer_reset(app->timer);
        break_overlay_hide(app->break_overlay);
    } else if (g_strcmp0(action, "extend_break") == 0) {
        // Add 5 minutes (300 seconds) to the current break
        timer_extend_break(app->timer, 300);
    } else if (g_strcmp0(action, "pause") == 0) {
        TimerState current_state = timer_get_state(app->timer);
        
        if (current_state == TIMER_STATE_PAUSED) {
            // Resume the timer
            timer_start(app->timer);
            break_overlay_update_pause_button(app->break_overlay, "Pause");
            // The timer state callback will update the overlay type back to break type
        } else {
            // Pause the timer
            timer_pause(app->timer);
            break_overlay_update_type(app->break_overlay, "Paused");
            break_overlay_update_pause_button(app->break_overlay, "Resume");
        }
    } else if (g_strcmp0(action, "dismiss") == 0) {
        // Just hide the overlay (ESC key)
        break_overlay_hide(app->break_overlay);
    }
}

static void on_auto_start_toggled(GtkToggleButton *button, gpointer user_data) {
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    if (!app || !app->settings) return;
    
    // Update setting
    app->settings->auto_start_work_after_break = gtk_toggle_button_get_active(button);
    
    // Apply to timer
    timer_set_auto_start_work(app->timer, app->settings->auto_start_work_after_break);
    
    // Save settings (config manager handles in-memory vs persistent)
    if (app->config) {
        config_save_settings(app->config, app->settings);
    }
}

static void cleanup_app(GomodaroApp *app) {
    if (!app) return;
    
    // Stop idle monitoring
    stop_idle_monitoring(app);
    
    // Save settings one final time (config manager handles in-memory vs persistent)
    if (app->settings && app->config) {
        config_save_settings(app->config, app->settings);
    }
    
    // Clean up resources
    if (app->timer) timer_free(app->timer);
    if (app->audio) audio_manager_free(app->audio);
    if (app->tray_icon) tray_icon_free(app->tray_icon);
    if (app->status_tray) tray_status_icon_free(app->status_tray);
    if (app->break_overlay) break_overlay_free(app->break_overlay);
    if (app->input_monitor) input_monitor_free(app->input_monitor);
    if (app->dbus_service) dbus_service_free(app->dbus_service);
    if (app->settings) settings_free(app->settings);
    if (app->config) config_free(app->config);
    
    g_free(app);
}

static gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)widget; // Suppress unused parameter warning
    (void)event;  // Suppress unused parameter warning
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    // Hide the window instead of destroying it
    gtk_widget_hide(app->window);
    
    // Return TRUE to prevent the default handler from destroying the window
    return TRUE;
}

static void quit_application(GomodaroApp *app) {
    // Cleanup and quit the application
    cleanup_app(app);
    gtk_main_quit();
}

static void on_tray_auto_start_toggled(GtkCheckMenuItem *item, gpointer user_data) {
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    if (!app || !app->settings || !item) return;
    
    // Update setting
    gboolean active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
    app->settings->auto_start_work_after_break = active;
    
    // Apply to timer
    timer_set_auto_start_work(app->timer, active);
    
    // Update main window checkbox to stay in sync
    if (app->auto_start_check && GTK_IS_TOGGLE_BUTTON(app->auto_start_check)) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->auto_start_check), active);
    }
    
    // Save settings
    if (app->config) {
        config_save_settings(app->config, app->settings);
    }
}

static void on_tray_config_clicked(GtkMenuItem *item, gpointer user_data) {
    (void)item; // Suppress unused parameter warning
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    // Open settings dialog (same as clicking settings button in main window)
    on_settings_clicked(NULL, app);
}

static void on_tray_start_clicked(GtkMenuItem *item, gpointer user_data) {
    (void)item; // Suppress unused parameter warning
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    TimerState state = timer_get_state(app->timer);
    
    // Single button logic: Start/Pause/Resume
    if (state == TIMER_STATE_IDLE || state == TIMER_STATE_PAUSED) {
        timer_start(app->timer);
    } else if (state == TIMER_STATE_WORK || state == TIMER_STATE_SHORT_BREAK || 
               state == TIMER_STATE_LONG_BREAK) {
        timer_pause(app->timer);
    }
}

static void on_tray_reset_clicked(GtkMenuItem *item, gpointer user_data) {
    (void)item; // Suppress unused parameter warning
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    timer_reset(app->timer);
}

static gboolean on_input_activity_detected(gpointer user_data) {
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    g_print("on_input_activity_detected called!\n");
    
    if (!app || !app->timer) {
        g_print("on_input_activity_detected: app or timer is NULL\n");
        return G_SOURCE_REMOVE;
    }
    
    TimerState current_state = timer_get_state(app->timer);
    g_print("on_input_activity_detected: current_state=%d, auto_start=%d, paused_by_idle=%d\n", 
            current_state, app->settings ? app->settings->auto_start_work_after_break : 0,
            app->paused_by_idle);
    
    // Handle auto-resume from idle pause
    if (current_state == TIMER_STATE_PAUSED && app->paused_by_idle) {
        g_print("Auto-resuming work session after idle pause\n");
        app->paused_by_idle = FALSE;
        
        timer_start(app->timer);
        
        // Show notification that we're resuming
        if (!gtk_widget_get_visible(app->window)) {
            gtk_widget_show(app->window);
            gtk_window_present(GTK_WINDOW(app->window));
            gtk_window_set_urgency_hint(GTK_WINDOW(app->window), TRUE);
        }
        
        return G_SOURCE_REMOVE;
    }
    
    // Handle auto-start after break
    if (current_state == TIMER_STATE_IDLE && 
        app->settings && app->settings->auto_start_work_after_break) {
        
        g_print("Auto-starting work session from input activity\n");
        
        // Ensure input monitoring is stopped to prevent multiple triggers
        if (app->input_monitor) {
            input_monitor_stop(app->input_monitor);
        }
        
        // Start work session
        timer_start(app->timer);
    }
    
    return G_SOURCE_REMOVE; // Remove this idle source
}

static gboolean check_idle_timeout(gpointer user_data) {
    GomodaroApp *app = (GomodaroApp *)user_data;
    
    if (!app || !app->input_monitor || !app->settings) {
        return G_SOURCE_CONTINUE;
    }
    
    // Only check idle time during work sessions
    TimerState state = timer_get_state(app->timer);
    if (state != TIMER_STATE_WORK) {
        return G_SOURCE_CONTINUE;
    }
    
    // Get current idle time
    int idle_seconds = input_monitor_get_idle_time(app->input_monitor);
    if (idle_seconds < 0) {
        // Error getting idle time, continue checking
        return G_SOURCE_CONTINUE;
    }
    
    int idle_timeout_seconds = app->settings->idle_timeout_minutes * 60;
    
    g_print("Idle check: %d seconds idle (timeout: %d seconds)\n", idle_seconds, idle_timeout_seconds);
    
    if (idle_seconds >= idle_timeout_seconds) {
        g_print("Idle timeout reached, pausing timer\n");
        
        // Pause the timer due to idle
        app->paused_by_idle = TRUE;
        timer_pause(app->timer);
        
        // Play idle pause sound
        audio_manager_play_idle_pause(app->audio);
        
        // Start monitoring for activity to resume
        input_monitor_start(app->input_monitor);
        
        // Update tooltip to indicate idle pause
        tray_icon_set_tooltip(app->tray_icon, "Commodoro - Paused (idle)");
        
        // Update status tray with idle pause indication
        cairo_surface_t *surface = tray_icon_get_surface(app->tray_icon);
        if (surface) {
            tray_status_icon_update(app->status_tray, surface, "Commodoro - Paused (idle)");
        }
    }
    
    return G_SOURCE_CONTINUE;
}

static void start_idle_monitoring(GomodaroApp *app) {
    if (!app || !app->settings || !app->settings->enable_idle_detection) {
        return;
    }
    
    // Stop any existing idle check
    stop_idle_monitoring(app);
    
    // Start periodic idle checking (every 30 seconds)
    app->idle_check_source = g_timeout_add_seconds(30, check_idle_timeout, app);
    g_print("Started idle monitoring (checking every 30 seconds)\n");
}

static void stop_idle_monitoring(GomodaroApp *app) {
    if (app && app->idle_check_source > 0) {
        g_source_remove(app->idle_check_source);
        app->idle_check_source = 0;
        g_print("Stopped idle monitoring\n");
    }
}

int main(int argc, char *argv[]) {
    gboolean auto_start = FALSE;
    const char *dbus_command = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--auto-start") == 0) {
            auto_start = TRUE;
        } else {
            // Check if it's a D-Bus command
            const char *command = dbus_parse_command(argv[i]);
            if (command) {
                dbus_command = command;
            }
        }
    }

    // Handle D-Bus commands
    if (dbus_command) {
        DBusCommandResult result = dbus_send_command(dbus_command, auto_start, NULL);
        
        switch (result) {
            case DBUS_RESULT_SUCCESS:
                return 0;
                
            case DBUS_RESULT_START_NEEDED:
                // Start the app and execute the command after startup
                g_print("Starting Commodoro...\n");
                g_setenv("COMMODORO_STARTUP_CMD", dbus_command, TRUE);
                break;
                
            case DBUS_RESULT_NOT_RUNNING:
            case DBUS_RESULT_ERROR:
                return 1;
        }
    }

    
    // Parse command line arguments
    CmdLineArgs *cmd_args = parse_command_line(argc, argv);
    
    // Initialize GTK
    gtk_init(&argc, &argv);
    
    // Call activate directly
    activate(NULL, cmd_args);
    
    // Start main loop
    gtk_main();
    
    // Cleanup
    g_free(cmd_args);
    
    return 0;
}

// Callback for delayed window presentation to combat focus stealing
static gboolean delayed_window_present(gpointer user_data) {
    GtkWindow *window = GTK_WINDOW(user_data);
    
    if (window && GTK_IS_WINDOW(window)) {
        gtk_window_present(window);
        gtk_window_set_urgency_hint(window, FALSE); // Clear urgency hint after presentation
    }
    
    return FALSE; // Remove timeout (one-shot)
}