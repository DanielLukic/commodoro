#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>
#include "settings_dialog.h"

G_BEGIN_DECLS

/**
 * Configuration manager for settings
 */
typedef struct _Config Config;

/**
 * Creates a new config manager
 * @param use_persistent TRUE for file-based config, FALSE for in-memory only
 * @return New Config instance
 */
Config* config_new(gboolean use_persistent);

/**
 * Frees a config manager
 * @param config Config instance to free
 */
void config_free(Config *config);

/**
 * Loads settings from disk
 * @param config Config instance
 * @return Settings loaded from disk, or default settings if file doesn't exist
 */
Settings* config_load_settings(Config *config);

/**
 * Saves settings to disk
 * @param config Config instance
 * @param settings Settings to save
 * @return TRUE if successful, FALSE otherwise
 */
gboolean config_save_settings(Config *config, const Settings *settings);

/**
 * Gets the config directory path
 * @param config Config instance
 * @return Config directory path (caller should not free)
 */
const char* config_get_config_dir(Config *config);

G_END_DECLS

#endif // CONFIG_H