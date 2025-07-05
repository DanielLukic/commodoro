#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct DBusService DBusService;

extern void on_start_clicked(void *widget, void *app);

extern void on_reset_clicked(void *widget, void *app);

extern void timer_skip_phase(void *timer);

extern uint32_t timer_get_state(void *timer);

extern int32_t gtk_widget_get_visible(void *widget);

extern void gtk_widget_hide(void *widget);

extern void gtk_widget_show(void *widget);

extern void gtk_window_present(void *window);

struct DBusService *dbus_service_new(void *app_pointer);

void dbus_service_free(struct DBusService *service);

void dbus_service_publish(struct DBusService *service);

void dbus_service_unpublish(struct DBusService *service);
