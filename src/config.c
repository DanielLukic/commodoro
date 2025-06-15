#include "config.h"
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

struct _Config {
    char *config_dir;
    char *config_file;
    gboolean use_persistent;  // TRUE for file-based, FALSE for in-memory only
};

static gboolean ensure_config_directory(const char *config_dir);
static Settings* parse_config_file(const char *config_file);
static gboolean write_config_file(const char *config_file, const Settings *settings);
static char* escape_json_string(const char *str);

Config* config_new(gboolean use_persistent) {
    Config *config = g_malloc0(sizeof(Config));
    
    config->use_persistent = use_persistent;
    
    if (use_persistent) {
        // Create config directory path: ~/.config/commodoro/
        config->config_dir = g_build_filename(g_get_user_config_dir(), "commodoro", NULL);
        config->config_file = g_build_filename(config->config_dir, "config.json", NULL);
    } else {
        // In-memory config for test mode
        config->config_dir = NULL;
        config->config_file = NULL;
    }
    
    return config;
}

void config_free(Config *config) {
    if (!config) return;
    
    g_free(config->config_dir);
    g_free(config->config_file);
    g_free(config);
}

Settings* config_load_settings(Config *config) {
    if (!config) return settings_new_default();
    
    // In-memory config always returns defaults
    if (!config->use_persistent) {
        g_print("Using in-memory config (test mode)\n");
        return settings_new_default();
    }
    
    // Ensure config directory exists
    if (!ensure_config_directory(config->config_dir)) {
        g_warning("Failed to create config directory: %s", config->config_dir);
        return settings_new_default();
    }
    
    // Check if config file exists
    if (!g_file_test(config->config_file, G_FILE_TEST_EXISTS)) {
        g_print("Config file not found, using defaults: %s\n", config->config_file);
        return settings_new_default();
    }
    
    // Parse config file
    Settings *settings = parse_config_file(config->config_file);
    if (!settings) {
        g_warning("Failed to parse config file, using defaults: %s", config->config_file);
        return settings_new_default();
    }
    
    g_print("Loaded settings from: %s\n", config->config_file);
    return settings;
}

gboolean config_save_settings(Config *config, const Settings *settings) {
    if (!config || !settings) return FALSE;
    
    // In-memory config doesn't save to disk
    if (!config->use_persistent) {
        g_print("In-memory config: settings not saved to disk\n");
        return TRUE;  // Success, but no-op
    }
    
    // Ensure config directory exists
    if (!ensure_config_directory(config->config_dir)) {
        g_warning("Failed to create config directory: %s", config->config_dir);
        return FALSE;
    }
    
    // Write config file
    if (!write_config_file(config->config_file, settings)) {
        g_warning("Failed to write config file: %s", config->config_file);
        return FALSE;
    }
    
    g_print("Saved settings to: %s\n", config->config_file);
    return TRUE;
}

const char* config_get_config_dir(Config *config) {
    if (!config) return NULL;
    return config->config_dir;
}

static gboolean ensure_config_directory(const char *config_dir) {
    if (!config_dir) return FALSE;
    
    if (g_file_test(config_dir, G_FILE_TEST_IS_DIR)) {
        return TRUE;
    }
    
    return g_mkdir_with_parents(config_dir, 0755) == 0;
}

static Settings* parse_config_file(const char *config_file) {
    if (!config_file) return NULL;
    
    // Read file contents
    char *contents = NULL;
    gsize length;
    GError *error = NULL;
    
    if (!g_file_get_contents(config_file, &contents, &length, &error)) {
        if (error) {
            g_warning("Failed to read config file: %s", error->message);
            g_error_free(error);
        }
        return NULL;
    }
    
    Settings *settings = settings_new_default();
    
    // Parse JSON manually (simple key-value parsing)
    char *line = strtok(contents, "\n");
    while (line != NULL) {
        // Skip whitespace and comments
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '{' || *line == '}' || *line == '\0' || *line == '/' || *line == '#') {
            line = strtok(NULL, "\n");
            continue;
        }
        
        // Look for key-value pairs
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = line;
            char *value = colon + 1;
            
            // Clean up key (remove quotes and whitespace)
            while (*key == ' ' || *key == '\t' || *key == '\"') key++;
            char *key_end = key + strlen(key) - 1;
            while (key_end > key && (*key_end == ' ' || *key_end == '\t' || *key_end == '\"' || *key_end == ',')) {
                *key_end = '\0';
                key_end--;
            }
            
            // Clean up value (remove quotes, whitespace, and comma)
            while (*value == ' ' || *value == '\t' || *value == '\"') value++;
            char *value_end = value + strlen(value) - 1;
            while (value_end > value && (*value_end == ' ' || *value_end == '\t' || *value_end == '\"' || *value_end == ',' || *value_end == '\r')) {
                *value_end = '\0';
                value_end--;
            }
            
            // Parse known settings
            if (strcmp(key, "work_duration") == 0) {
                settings->work_duration = atoi(value);
            } else if (strcmp(key, "short_break_duration") == 0) {
                settings->short_break_duration = atoi(value);
            } else if (strcmp(key, "long_break_duration") == 0) {
                settings->long_break_duration = atoi(value);
            } else if (strcmp(key, "sessions_until_long_break") == 0) {
                settings->sessions_until_long_break = atoi(value);
            } else if (strcmp(key, "auto_start_work_after_break") == 0) {
                settings->auto_start_work_after_break = (strcmp(value, "true") == 0);
            } else if (strcmp(key, "enable_sounds") == 0) {
                settings->enable_sounds = (strcmp(value, "true") == 0);
            } else if (strcmp(key, "sound_volume") == 0) {
                settings->sound_volume = atof(value);
            } else if (strcmp(key, "sound_type") == 0) {
                g_free(settings->sound_type);
                settings->sound_type = g_strdup(value);
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    g_free(contents);
    return settings;
}

static gboolean write_config_file(const char *config_file, const Settings *settings) {
    if (!config_file || !settings) return FALSE;
    
    FILE *file = fopen(config_file, "w");
    if (!file) {
        return FALSE;
    }
    
    // Write JSON format
    fprintf(file, "{\n");
    fprintf(file, "  \"work_duration\": %d,\n", settings->work_duration);
    fprintf(file, "  \"short_break_duration\": %d,\n", settings->short_break_duration);
    fprintf(file, "  \"long_break_duration\": %d,\n", settings->long_break_duration);
    fprintf(file, "  \"sessions_until_long_break\": %d,\n", settings->sessions_until_long_break);
    fprintf(file, "  \"auto_start_work_after_break\": %s,\n", settings->auto_start_work_after_break ? "true" : "false");
    fprintf(file, "  \"enable_sounds\": %s,\n", settings->enable_sounds ? "true" : "false");
    fprintf(file, "  \"sound_volume\": %.2f", settings->sound_volume);
    
    if (settings->sound_type) {
        char *escaped = escape_json_string(settings->sound_type);
        fprintf(file, ",\n  \"sound_type\": \"%s\"", escaped);
        g_free(escaped);
    }
    
    if (settings->work_start_sound) {
        char *escaped = escape_json_string(settings->work_start_sound);
        fprintf(file, ",\n  \"work_start_sound\": \"%s\"", escaped);
        g_free(escaped);
    }
    
    if (settings->break_start_sound) {
        char *escaped = escape_json_string(settings->break_start_sound);
        fprintf(file, ",\n  \"break_start_sound\": \"%s\"", escaped);
        g_free(escaped);
    }
    
    if (settings->session_complete_sound) {
        char *escaped = escape_json_string(settings->session_complete_sound);
        fprintf(file, ",\n  \"session_complete_sound\": \"%s\"", escaped);
        g_free(escaped);
    }
    
    if (settings->timer_finish_sound) {
        char *escaped = escape_json_string(settings->timer_finish_sound);
        fprintf(file, ",\n  \"timer_finish_sound\": \"%s\"", escaped);
        g_free(escaped);
    }
    
    fprintf(file, "\n}\n");
    
    fclose(file);
    return TRUE;
}

static char* escape_json_string(const char *str) {
    if (!str) return g_strdup("");
    
    // Simple escaping - just escape quotes and backslashes
    GString *escaped = g_string_new("");
    for (const char *p = str; *p; p++) {
        if (*p == '\"' || *p == '\\') {
            g_string_append_c(escaped, '\\');
        }
        g_string_append_c(escaped, *p);
    }
    
    char *result = g_strdup(escaped->str);
    g_string_free(escaped, TRUE);
    return result;
}