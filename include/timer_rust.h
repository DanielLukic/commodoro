#ifndef TIMER_RUST_H
#define TIMER_RUST_H

#include <glib.h>

G_BEGIN_DECLS

// Map Rust enum values to match C enum
typedef enum {
    TIMER_STATE_IDLE = 0,
    TIMER_STATE_WORK = 1,
    TIMER_STATE_SHORT_BREAK = 2,
    TIMER_STATE_LONG_BREAK = 3,
    TIMER_STATE_PAUSED = 4
} TimerState;

typedef struct Timer Timer;

// Callback typedefs matching the original C API
typedef void (*TimerStateCallback)(Timer *timer, TimerState state, gpointer user_data);
typedef void (*TimerTickCallback)(Timer *timer, int minutes, int seconds, gpointer user_data);
typedef void (*TimerSessionCompleteCallback)(Timer *timer, TimerState completed_state, gpointer user_data);

// Functions from Rust library
Timer* timer_new(void);
void timer_free(Timer *timer);
void timer_set_durations(Timer *timer, int work_duration, int short_break_duration, 
                        int long_break_duration, int sessions_until_long);
void timer_set_callbacks(Timer *timer, TimerStateCallback state_cb, 
                        TimerTickCallback tick_cb, TimerSessionCompleteCallback session_complete_cb,
                        gpointer user_data);
void timer_start(Timer *timer);
void timer_pause(Timer *timer);
void timer_reset(Timer *timer);
TimerState timer_get_state(Timer *timer);
int timer_get_session(Timer *timer);
void timer_get_remaining(Timer *timer, int *minutes, int *seconds);
int timer_get_total_duration(Timer *timer);
void timer_extend_break(Timer *timer, int additional_seconds);
void timer_set_auto_start_work(Timer *timer, gboolean auto_start);
void timer_set_duration_mode(Timer *timer, gboolean use_seconds);
void timer_skip_phase(Timer *timer);

// GLib timer callback
gboolean timer_tick_callback(gpointer user_data);

G_END_DECLS

#endif // TIMER_RUST_H