#ifndef DBUS_H
#define DBUS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    DBUS_RESULT_SUCCESS,        // Command sent successfully
    DBUS_RESULT_NOT_RUNNING,    // Commodoro not running (and auto-start not requested)
    DBUS_RESULT_START_NEEDED,   // Commodoro not running but auto-start requested
    DBUS_RESULT_ERROR           // Other error occurred
} DBusCommandResult;

/**
 * Sends a D-Bus command to a running Commodoro instance.
 * 
 * @param command The D-Bus method name to call (e.g., "ToggleTimer", "ResetTimer", etc.)
 * @param auto_start If TRUE and Commodoro is not running, returns DBUS_RESULT_START_NEEDED
 * @param unused Reserved for future use, pass NULL
 * @return DBusCommandResult indicating the outcome
 */
DBusCommandResult dbus_send_command(const char *command, gboolean auto_start, void *unused);

/**
 * Checks if a string is a valid D-Bus command.
 * 
 * @param str The string to check
 * @return The D-Bus method name if valid, NULL otherwise
 */
const char* dbus_parse_command(const char *str);

G_END_DECLS

#endif // DBUS_H