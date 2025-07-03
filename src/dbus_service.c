#include "dbus_service.h"
#include <gio/gio.h>
#include "app.h"
#include "callbacks.h"

struct _DBusService {
    GDBusConnection *connection;
    guint owner_id;
    gpointer app_pointer;
};

static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data);
static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data);
static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data);

static void handle_method_call(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data);

static const GDBusInterfaceVTable interface_vtable = {
    .method_call = handle_method_call
};

DBusService* dbus_service_new(gpointer app_pointer) {
    DBusService *service = g_malloc0(sizeof(DBusService));
    service->app_pointer = app_pointer;
    return service;
}

void dbus_service_free(DBusService *service) {
    if (!service) return;
    dbus_service_unpublish(service);
    g_free(service);
}

void dbus_service_publish(DBusService *service) {
    service->owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                                     "org.dl.commodoro",
                                     G_BUS_NAME_OWNER_FLAGS_NONE,
                                     on_bus_acquired,
                                     on_name_acquired,
                                     on_name_lost,
                                     service,
                                     NULL);
}

void dbus_service_unpublish(DBusService *service) {
    if (service->owner_id) {
        g_bus_unown_name(service->owner_id);
        service->owner_id = 0;
    }
}

static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    DBusService *service = (DBusService*)user_data;
    service->connection = connection;

    // For now, we don't register any objects. This will be done in on_name_acquired.
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    DBusService *service = (DBusService*)user_data;
    GError *error = NULL;

    // This is a simplified introspection XML. A real implementation would be more detailed.
    const gchar *introspection_xml = 
        "<node>"
        "  <interface name='org.dl.commodoro.Timer'>"
        "    <method name='ToggleTimer'/>"
        "    <method name='ResetTimer'/>"
        "    <method name='ToggleBreak'/>"
        "    <method name='ShowHide'/>"
        "    <method name='GetState'>"
        "      <arg type='s' name='state' direction='out'/>"
        "    </method>"
        "  </interface>"
        "</node>";

    GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    if (error) {
        g_warning("Failed to create introspection data: %s", error->message);
        g_error_free(error);
        return;
    }

    g_dbus_connection_register_object(connection,
                                      "/org/dl/commodoro",
                                      introspection_data->interfaces[0],
                                      &interface_vtable,
                                      service,
                                      NULL, // GDestroyNotify
                                      &error);

    g_dbus_node_info_unref(introspection_data);

    if (error) {
        g_warning("Failed to register D-Bus object: %s", error->message);
        g_error_free(error);
    }
}

static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    // Handle the case where we lose the bus name
    g_printerr("Commodoro is already running. Use 'commodoro show_hide' to show the window.\n");
    exit(1);
}

static void handle_method_call(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data) {
    DBusService *service = (DBusService*)user_data;
    GomodaroApp *app = (GomodaroApp*)service->app_pointer;

    if (g_strcmp0(method_name, "ToggleTimer") == 0) {
        on_start_clicked(NULL, app);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "ResetTimer") == 0) {
        on_reset_clicked(NULL, app);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "ToggleBreak") == 0) {
        timer_skip_phase(app->timer);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "ShowHide") == 0) {
        if (gtk_widget_get_visible(app->window)) {
            gtk_widget_hide(app->window);
        } else {
            gtk_widget_show(app->window);
            gtk_window_present(GTK_WINDOW(app->window));
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "GetState") == 0) {
        TimerState state = timer_get_state(app->timer);
        const char *state_str;
        switch (state) {
            case TIMER_STATE_IDLE: state_str = "IDLE"; break;
            case TIMER_STATE_WORK: state_str = "WORK"; break;
            case TIMER_STATE_SHORT_BREAK: state_str = "SHORT_BREAK"; break;
            case TIMER_STATE_LONG_BREAK: state_str = "LONG_BREAK"; break;
            case TIMER_STATE_PAUSED: state_str = "PAUSED"; break;
            default: state_str = "UNKNOWN"; break;
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", state_str));
    } else {
        g_dbus_method_invocation_return_dbus_error(invocation, "org.freedesktop.DBus.Error.UnknownMethod", "Method does not exist");
    }
}
