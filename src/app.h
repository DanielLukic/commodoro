#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>
#include "tray_icon.h"
#include "timer_rust.h"
#include "tray_status_icon.h"
#include "audio.h"
#include "settings_dialog.h"
#include "break_overlay.h"
#include "config.h"
#include "input_monitor.h"
#include "dbus_service.h"

typedef struct {
    int work_duration;           // in minutes
    int short_break_duration;    // in minutes  
    int long_break_duration;     // in minutes
    int sessions_until_long_break;
    gboolean test_mode;          // TRUE if custom durations provided
} CmdLineArgs;

typedef struct {
    GtkWidget *window;
    GtkWidget *time_label;
    GtkWidget *status_label;
    GtkWidget *session_label;
    GtkWidget *start_button;
    GtkWidget *pause_button;
    GtkWidget *reset_button;
    GtkWidget *settings_button;
    GtkWidget *auto_start_check;
    
    TrayIcon *tray_icon;
    TrayStatusIcon *status_tray;
    Timer *timer;
    AudioManager *audio;
    Settings *settings;
    BreakOverlay *break_overlay;
    Config *config;              // Configuration manager
    InputMonitor *input_monitor; // User activity monitor
    DBusService *dbus_service;   // D-Bus service
    CmdLineArgs *args;           // Command line arguments
    guint idle_check_source;     // Idle detection timer source
    gboolean paused_by_idle;     // Track if timer was paused due to idle
} GomodaroApp;

#endif // APP_H
