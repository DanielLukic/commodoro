#include "settings_dialog.h"
#include <string.h>

struct _SettingsDialog {
    GtkWidget *dialog;
    GtkWidget *notebook;
    
    // Timer tab widgets
    GtkWidget *work_duration_spin;
    GtkWidget *short_break_spin;
    GtkWidget *long_break_spin;
    GtkWidget *sessions_spin;
    
    // Misc tab widgets
    GtkWidget *auto_start_check;
    GtkWidget *enable_sounds_check;
    GtkWidget *enable_idle_detection_check;
    GtkWidget *idle_timeout_spin;
    GtkWidget *idle_timeout_box;
    
    // Dialog buttons
    GtkWidget *restore_defaults_button;
    GtkWidget *cancel_button;
    GtkWidget *ok_button;
    
    SettingsDialogCallback callback;
    gpointer user_data;
};

static void on_restore_defaults_clicked(GtkButton *button, SettingsDialog *dialog);
static void on_cancel_clicked(GtkButton *button, SettingsDialog *dialog);
static void on_ok_clicked(GtkButton *button, SettingsDialog *dialog);
static void on_idle_detection_toggled(GtkToggleButton *button, SettingsDialog *dialog);

SettingsDialog* settings_dialog_new(GtkWindow *parent, const Settings *settings, AudioManager *audio) {
    (void)audio; // Not used in simplified version
    SettingsDialog *dialog = g_malloc0(sizeof(SettingsDialog));
    
    // Create dialog
    dialog->dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog->dialog), "Settings");
    gtk_window_set_transient_for(GTK_WINDOW(dialog->dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog->dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog->dialog), 400, 300);
    gtk_window_set_resizable(GTK_WINDOW(dialog->dialog), FALSE);
    
    // Create main container
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog->dialog));
    
    // Create notebook for tabs
    dialog->notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(content_area), dialog->notebook);
    
    // Create Timer tab
    GtkWidget *timer_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(timer_tab), 20);
    
    GtkWidget *timer_label = gtk_label_new("Timer");
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook), timer_tab, timer_label);
    
    // Timer Durations section
    GtkWidget *durations_frame = gtk_frame_new("Timer Durations");
    gtk_box_pack_start(GTK_BOX(timer_tab), durations_frame, FALSE, FALSE, 0);
    
    GtkWidget *durations_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(durations_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(durations_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(durations_grid), 15);
    gtk_container_add(GTK_CONTAINER(durations_frame), durations_grid);
    
    // Work Duration
    GtkWidget *work_label = gtk_label_new("Work Duration:");
    gtk_widget_set_halign(work_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(durations_grid), work_label, 0, 0, 1, 1);
    
    dialog->work_duration_spin = gtk_spin_button_new_with_range(1, 120, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->work_duration_spin), settings->work_duration);
    gtk_grid_attach(GTK_GRID(durations_grid), dialog->work_duration_spin, 1, 0, 1, 1);
    
    GtkWidget *work_min_label = gtk_label_new("min");
    gtk_grid_attach(GTK_GRID(durations_grid), work_min_label, 2, 0, 1, 1);
    
    // Short Break
    GtkWidget *short_break_label = gtk_label_new("Short Break:");
    gtk_widget_set_halign(short_break_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(durations_grid), short_break_label, 0, 1, 1, 1);
    
    dialog->short_break_spin = gtk_spin_button_new_with_range(1, 60, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->short_break_spin), settings->short_break_duration);
    gtk_grid_attach(GTK_GRID(durations_grid), dialog->short_break_spin, 1, 1, 1, 1);
    
    GtkWidget *short_min_label = gtk_label_new("min");
    gtk_grid_attach(GTK_GRID(durations_grid), short_min_label, 2, 1, 1, 1);
    
    // Long Break
    GtkWidget *long_break_label = gtk_label_new("Long Break:");
    gtk_widget_set_halign(long_break_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(durations_grid), long_break_label, 0, 2, 1, 1);
    
    dialog->long_break_spin = gtk_spin_button_new_with_range(5, 120, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->long_break_spin), settings->long_break_duration);
    gtk_grid_attach(GTK_GRID(durations_grid), dialog->long_break_spin, 1, 2, 1, 1);
    
    GtkWidget *long_min_label = gtk_label_new("min");
    gtk_grid_attach(GTK_GRID(durations_grid), long_min_label, 2, 2, 1, 1);
    
    // Sessions until long break
    GtkWidget *sessions_label = gtk_label_new("Sessions until Long Break:");
    gtk_widget_set_halign(sessions_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(durations_grid), sessions_label, 0, 3, 1, 1);
    
    dialog->sessions_spin = gtk_spin_button_new_with_range(2, 10, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->sessions_spin), settings->sessions_until_long_break);
    gtk_grid_attach(GTK_GRID(durations_grid), dialog->sessions_spin, 1, 3, 1, 1);
    
    // Create Misc tab
    GtkWidget *misc_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(misc_tab), 20);
    
    GtkWidget *misc_label = gtk_label_new("Misc");
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook), misc_tab, misc_label);
    
    // Behavior Settings
    GtkWidget *behavior_frame = gtk_frame_new("Behavior Settings");
    gtk_box_pack_start(GTK_BOX(misc_tab), behavior_frame, FALSE, FALSE, 0);
    
    GtkWidget *behavior_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(behavior_box), 15);
    gtk_container_add(GTK_CONTAINER(behavior_frame), behavior_box);
    
    dialog->auto_start_check = gtk_check_button_new_with_label("Auto-start work after breaks end");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->auto_start_check), settings->auto_start_work_after_break);
    gtk_widget_set_margin_bottom(dialog->auto_start_check, 8);
    gtk_box_pack_start(GTK_BOX(behavior_box), dialog->auto_start_check, FALSE, FALSE, 0);
    
    dialog->enable_sounds_check = gtk_check_button_new_with_label("Enable sound alerts");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->enable_sounds_check), settings->enable_sounds);
    gtk_box_pack_start(GTK_BOX(behavior_box), dialog->enable_sounds_check, FALSE, FALSE, 0);
    
    // Idle detection section
    gtk_widget_set_margin_top(dialog->enable_sounds_check, 8);
    dialog->enable_idle_detection_check = gtk_check_button_new_with_label("Auto-pause when idle");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->enable_idle_detection_check), settings->enable_idle_detection);
    gtk_widget_set_margin_top(dialog->enable_idle_detection_check, 8);
    gtk_box_pack_start(GTK_BOX(behavior_box), dialog->enable_idle_detection_check, FALSE, FALSE, 0);
    
    // Idle timeout configuration
    dialog->idle_timeout_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_left(dialog->idle_timeout_box, 24);
    gtk_widget_set_margin_top(dialog->idle_timeout_box, 5);
    gtk_box_pack_start(GTK_BOX(behavior_box), dialog->idle_timeout_box, FALSE, FALSE, 0);
    
    GtkWidget *idle_timeout_label = gtk_label_new("Idle timeout:");
    gtk_box_pack_start(GTK_BOX(dialog->idle_timeout_box), idle_timeout_label, FALSE, FALSE, 0);
    
    dialog->idle_timeout_spin = gtk_spin_button_new_with_range(1, 30, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->idle_timeout_spin), settings->idle_timeout_minutes);
    gtk_box_pack_start(GTK_BOX(dialog->idle_timeout_box), dialog->idle_timeout_spin, FALSE, FALSE, 0);
    
    GtkWidget *idle_min_label = gtk_label_new("minutes");
    gtk_box_pack_start(GTK_BOX(dialog->idle_timeout_box), idle_min_label, FALSE, FALSE, 0);
    
    // Set sensitivity based on checkbox state
    gtk_widget_set_sensitive(dialog->idle_timeout_box, settings->enable_idle_detection);
    
    // Connect toggle signal to enable/disable timeout spinner
    g_signal_connect(dialog->enable_idle_detection_check, "toggled", 
                     G_CALLBACK(on_idle_detection_toggled), dialog);
    
    // Dialog buttons
    GtkWidget *action_area = gtk_dialog_get_action_area(GTK_DIALOG(dialog->dialog));
    
    dialog->restore_defaults_button = gtk_button_new_with_label("Restore Defaults");
    gtk_container_add(GTK_CONTAINER(action_area), dialog->restore_defaults_button);
    
    dialog->cancel_button = gtk_button_new_with_label("Cancel");
    gtk_container_add(GTK_CONTAINER(action_area), dialog->cancel_button);
    
    dialog->ok_button = gtk_button_new_with_label("OK");
    gtk_style_context_add_class(gtk_widget_get_style_context(dialog->ok_button), "suggested-action");
    gtk_container_add(GTK_CONTAINER(action_area), dialog->ok_button);
    
    // Connect signals
    g_signal_connect(dialog->restore_defaults_button, "clicked", G_CALLBACK(on_restore_defaults_clicked), dialog);
    g_signal_connect(dialog->cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), dialog);
    g_signal_connect(dialog->ok_button, "clicked", G_CALLBACK(on_ok_clicked), dialog);
    
    return dialog;
}

void settings_dialog_free(SettingsDialog *dialog) {
    if (!dialog) return;
    
    gtk_widget_destroy(dialog->dialog);
    g_free(dialog);
}

void settings_dialog_set_callback(SettingsDialog *dialog, SettingsDialogCallback callback, gpointer user_data) {
    if (!dialog) return;
    
    dialog->callback = callback;
    dialog->user_data = user_data;
}

void settings_dialog_show(SettingsDialog *dialog) {
    if (!dialog) return;
    
    gtk_widget_show_all(dialog->dialog);
}

Settings* settings_dialog_get_settings(SettingsDialog *dialog) {
    if (!dialog) return NULL;
    
    Settings *settings = g_malloc0(sizeof(Settings));
    
    // Timer settings
    settings->work_duration = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->work_duration_spin));
    settings->short_break_duration = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->short_break_spin));
    settings->long_break_duration = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->long_break_spin));
    settings->sessions_until_long_break = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->sessions_spin));
    
    // Behavior settings
    settings->auto_start_work_after_break = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->auto_start_check));
    settings->enable_idle_detection = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->enable_idle_detection_check));
    settings->idle_timeout_minutes = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->idle_timeout_spin));
    
    // Audio settings (simplified)
    settings->enable_sounds = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->enable_sounds_check));
    settings->sound_volume = 0.7; // Fixed reasonable volume
    settings->sound_type = g_strdup("chimes");
    settings->work_start_sound = NULL;
    settings->break_start_sound = NULL;
    settings->session_complete_sound = NULL;
    settings->timer_finish_sound = NULL;
    
    return settings;
}

Settings* settings_new_default(void) {
    Settings *settings = g_malloc0(sizeof(Settings));
    
    settings->work_duration = 25;
    settings->short_break_duration = 5;
    settings->long_break_duration = 15;
    settings->sessions_until_long_break = 4;
    settings->auto_start_work_after_break = TRUE;
    settings->enable_idle_detection = FALSE;  // Off by default
    settings->idle_timeout_minutes = 2;        // 2 minutes default
    settings->enable_sounds = TRUE;
    settings->sound_volume = 0.7; // Fixed reasonable volume
    settings->sound_type = g_strdup("chimes");
    settings->work_start_sound = NULL;
    settings->break_start_sound = NULL;
    settings->session_complete_sound = NULL;
    settings->timer_finish_sound = NULL;
    
    return settings;
}

void settings_free(Settings *settings) {
    if (!settings) return;
    
    g_free(settings->sound_type);
    g_free(settings->work_start_sound);
    g_free(settings->break_start_sound);
    g_free(settings->session_complete_sound);
    g_free(settings->timer_finish_sound);
    g_free(settings);
}

Settings* settings_copy(const Settings *settings) {
    if (!settings) return NULL;
    
    Settings *copy = g_malloc0(sizeof(Settings));
    
    copy->work_duration = settings->work_duration;
    copy->short_break_duration = settings->short_break_duration;
    copy->long_break_duration = settings->long_break_duration;
    copy->sessions_until_long_break = settings->sessions_until_long_break;
    copy->auto_start_work_after_break = settings->auto_start_work_after_break;
    copy->enable_idle_detection = settings->enable_idle_detection;
    copy->idle_timeout_minutes = settings->idle_timeout_minutes;
    copy->enable_sounds = settings->enable_sounds;
    copy->sound_volume = settings->sound_volume;
    copy->sound_type = g_strdup(settings->sound_type);
    copy->work_start_sound = g_strdup(settings->work_start_sound);
    copy->break_start_sound = g_strdup(settings->break_start_sound);
    copy->session_complete_sound = g_strdup(settings->session_complete_sound);
    copy->timer_finish_sound = g_strdup(settings->timer_finish_sound);
    
    return copy;
}

// Callback implementations
static void on_restore_defaults_clicked(GtkButton *button, SettingsDialog *dialog) {
    (void)button; // Suppress unused parameter warning
    
    Settings *defaults = settings_new_default();
    
    // Reset all controls to defaults
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->work_duration_spin), defaults->work_duration);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->short_break_spin), defaults->short_break_duration);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->long_break_spin), defaults->long_break_duration);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->sessions_spin), defaults->sessions_until_long_break);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->auto_start_check), defaults->auto_start_work_after_break);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->enable_sounds_check), defaults->enable_sounds);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->enable_idle_detection_check), defaults->enable_idle_detection);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->idle_timeout_spin), defaults->idle_timeout_minutes);
    
    settings_free(defaults);
}

static void on_cancel_clicked(GtkButton *button, SettingsDialog *dialog) {
    (void)button; // Suppress unused parameter warning
    
    if (dialog->callback) {
        dialog->callback("cancel", dialog->user_data);
    }
}

static void on_ok_clicked(GtkButton *button, SettingsDialog *dialog) {
    (void)button; // Suppress unused parameter warning
    
    if (dialog->callback) {
        dialog->callback("ok", dialog->user_data);
    }
}

static void on_idle_detection_toggled(GtkToggleButton *button, SettingsDialog *dialog) {
    gboolean active = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(dialog->idle_timeout_box, active);
}