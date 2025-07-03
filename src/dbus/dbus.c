#include "dbus.h"
#include <gio/gio.h>
#include <string.h>
#include <unistd.h>

#define DBUS_SERVICE_NAME "org.dl.zigodoro"
#define DBUS_OBJECT_PATH "/org/dl/zigodoro"
#define DBUS_INTERFACE_NAME "org.dl.zigodoro.Timer"

const char* dbus_parse_command(const char *str) {
    if (g_strcmp0(str, "toggle_timer") == 0) {
        return "ToggleTimer";
    } else if (g_strcmp0(str, "reset_timer") == 0) {
        return "ResetTimer";
    } else if (g_strcmp0(str, "toggle_break") == 0) {
        return "ToggleBreak";
    } else if (g_strcmp0(str, "show_hide") == 0) {
        return "ShowHide";
    }
    return NULL;
}

DBusCommandResult dbus_send_command(const char *command, gboolean auto_start, void *unused) {
    (void)unused; // Suppress unused parameter warning
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    DBusCommandResult result = DBUS_RESULT_ERROR;
    
    // Get D-Bus connection
    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!connection) {
        g_printerr("Could not connect to D-Bus: %s\n", error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        return DBUS_RESULT_ERROR;
    }
    
    // Try to call the method
    g_dbus_connection_call_sync(connection,
                                DBUS_SERVICE_NAME,
                                DBUS_OBJECT_PATH,
                                DBUS_INTERFACE_NAME,
                                command,
                                NULL, // no parameters
                                NULL, // no expected return type
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, // default timeout
                                NULL, // no cancellable
                                &error);
    
    if (error) {
        // Check if the service is not running
        if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN) ||
            g_error_matches(error, G_IO_ERROR, G_IO_ERROR_DBUS_ERROR)) {
            
            if (auto_start) {
                g_print("Zigodoro is not running.\n");
                result = DBUS_RESULT_START_NEEDED;
            } else {
                g_printerr("Zigodoro is not running. Use --auto-start to launch it.\n");
                result = DBUS_RESULT_NOT_RUNNING;
            }
        } else {
            g_printerr("D-Bus error: %s\n", error->message);
            result = DBUS_RESULT_ERROR;
        }
        
        g_error_free(error);
    } else {
        result = DBUS_RESULT_SUCCESS;
    }
    
    g_object_unref(connection);
    return result;
}