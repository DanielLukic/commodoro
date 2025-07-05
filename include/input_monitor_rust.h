#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct InputMonitor InputMonitor;

typedef struct Option_InputMonitorCallback Option_InputMonitorCallback;

struct InputMonitor *input_monitor_new(void);

void input_monitor_free(struct InputMonitor *monitor);

void input_monitor_set_callback(struct InputMonitor *monitor,
                                struct Option_InputMonitorCallback callback,
                                void *user_data);

void input_monitor_set_window(struct InputMonitor *_monitor, void *_window);

void input_monitor_start(struct InputMonitor *monitor);

void input_monitor_stop(struct InputMonitor *monitor);

int input_monitor_is_active(struct InputMonitor *monitor);

int input_monitor_get_idle_time(struct InputMonitor *monitor);
