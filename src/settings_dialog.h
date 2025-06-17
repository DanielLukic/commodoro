#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <gtk/gtk.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _SettingsDialog SettingsDialog;

/**
 * Callback function for settings dialog
 * @param action Action name ("ok", "cancel", "restore_defaults")
 * @param user_data User data passed to callback
 */
typedef void (*SettingsDialogCallback)(const char *action, gpointer user_data);

/**
 * Settings structure
 */
typedef struct {
    // Timer settings
    int work_duration;              // minutes (1-120)
    int short_break_duration;       // minutes (1-60)
    int long_break_duration;        // minutes (5-120)
    int sessions_until_long_break;  // count (2-10)
    
    // Behavior settings
    gboolean auto_start_work_after_break;
    gboolean enable_idle_detection;
    int idle_timeout_minutes;       // minutes (1-30)
    
    // Audio settings
    gboolean enable_sounds;
    double sound_volume;            // 0.0-1.0
    char *sound_type;               // "chimes" or "custom"
    char *work_start_sound;         // file path or NULL
    char *break_start_sound;        // file path or NULL
    char *session_complete_sound;   // file path or NULL
    char *timer_finish_sound;       // file path or NULL
} Settings;

// Forward declaration for AudioManager
typedef struct _AudioManager AudioManager;

/**
 * Creates a new settings dialog
 * @param parent Parent window
 * @param settings Current settings to display
 * @param audio AudioManager for testing sounds (optional)
 * @return New SettingsDialog instance
 */
SettingsDialog* settings_dialog_new(GtkWindow *parent, const Settings *settings, AudioManager *audio);

/**
 * Frees a settings dialog
 * @param dialog SettingsDialog instance to free
 */
void settings_dialog_free(SettingsDialog *dialog);

/**
 * Sets the callback for dialog actions
 * @param dialog SettingsDialog instance
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void settings_dialog_set_callback(SettingsDialog *dialog, SettingsDialogCallback callback, gpointer user_data);

/**
 * Shows the settings dialog
 * @param dialog SettingsDialog instance
 */
void settings_dialog_show(SettingsDialog *dialog);

/**
 * Gets the current settings from the dialog
 * @param dialog SettingsDialog instance
 * @return Settings structure (caller must free with settings_free)
 */
Settings* settings_dialog_get_settings(SettingsDialog *dialog);

/**
 * Creates default settings
 * @return Default Settings structure
 */
Settings* settings_new_default(void);

/**
 * Frees a settings structure
 * @param settings Settings to free
 */
void settings_free(Settings *settings);

/**
 * Copies settings structure
 * @param settings Settings to copy
 * @return Copy of settings
 */
Settings* settings_copy(const Settings *settings);

G_END_DECLS

#endif // SETTINGS_DIALOG_H