#ifndef CONFIG_RUST_H
#define CONFIG_RUST_H

#include <glib.h>

G_BEGIN_DECLS

// Forward declarations
typedef struct Config Config;
typedef struct Settings Settings;

// Config functions
Config* config_new(gboolean use_persistent);
void config_free(Config *config);
Settings* config_load_settings(Config *config);
gboolean config_save_settings(Config *config, const Settings *settings);
const char* config_get_config_dir(Config *config);

// Settings functions  
Settings* settings_new_default(void);
void settings_free(Settings *settings);
Settings* settings_copy(const Settings *settings);

// Settings getters
int settings_get_work_duration(const Settings *settings);
int settings_get_short_break_duration(const Settings *settings);
int settings_get_long_break_duration(const Settings *settings);
int settings_get_sessions_until_long_break(const Settings *settings);
gboolean settings_get_auto_start_work_after_break(const Settings *settings);
gboolean settings_get_enable_idle_detection(const Settings *settings);
int settings_get_idle_timeout_minutes(const Settings *settings);
gboolean settings_get_enable_sounds(const Settings *settings);
double settings_get_sound_volume(const Settings *settings);
const char* settings_get_sound_type(const Settings *settings);

// Settings setters for strings
void settings_set_sound_type(Settings *settings, const char *value);
void settings_set_work_start_sound(Settings *settings, const char *value);
void settings_set_break_start_sound(Settings *settings, const char *value);

// Free strings allocated by Rust
void rust_free_string(char *s);

// Direct access to struct fields for C compatibility
struct Settings {
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
};

G_END_DECLS

#endif // CONFIG_RUST_H