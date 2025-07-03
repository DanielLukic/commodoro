#ifndef DBUS_SERVICE_H
#define DBUS_SERVICE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _DBusService DBusService;

/**
 * Creates a new D-Bus service instance.
 * @param app_pointer A pointer to the main GomodaroApp instance.
 * @return A new DBusService object.
 */
DBusService* dbus_service_new(gpointer app_pointer);

/**
 * Frees a D-Bus service instance.
 * @param service The DBusService instance to free.
 */
void dbus_service_free(DBusService *service);

/**
 * Publishes the D-Bus service.
 * @param service The DBusService instance.
 */
void dbus_service_publish(DBusService *service);

/**
 * Unpublishes the D-Bus service.
 * @param service The DBusService instance.
 */
void dbus_service_unpublish(DBusService *service);

G_END_DECLS

#endif // DBUS_SERVICE_H
