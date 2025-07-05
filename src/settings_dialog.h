#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <gtk/gtk.h>
#include <glib.h>
#include "config_rust.h"
#include "audio_rust.h"

G_BEGIN_DECLS

typedef struct _SettingsDialog SettingsDialog;

/**
 * Callback function for settings dialog
 * @param action Action name ("ok", "cancel", "restore_defaults")
 * @param user_data User data passed to callback
 */
typedef void (*SettingsDialogCallback)(const char *action, gpointer user_data);

// Settings structure is now defined in config_rust.h
// AudioManager is now defined in audio_rust.h

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

// Settings management functions are now provided by config_rust.h

G_END_DECLS

#endif // SETTINGS_DIALOG_H