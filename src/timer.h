#ifndef TIMER_H
#define TIMER_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    TIMER_STATE_IDLE,
    TIMER_STATE_WORK,
    TIMER_STATE_SHORT_BREAK,
    TIMER_STATE_LONG_BREAK,
    TIMER_STATE_PAUSED
} TimerState;

typedef struct _Timer Timer;

/**
 * Callback function for timer state changes
 * @param timer Timer instance
 * @param state New timer state
 * @param user_data User data passed to callback
 */
typedef void (*TimerStateCallback)(Timer *timer, TimerState state, gpointer user_data);

/**
 * Callback function for timer tick updates
 * @param timer Timer instance
 * @param minutes Minutes remaining
 * @param seconds Seconds remaining
 * @param user_data User data passed to callback
 */
typedef void (*TimerTickCallback)(Timer *timer, int minutes, int seconds, gpointer user_data);

/**
 * Callback function for session completion events (work/break finished)
 * @param timer Timer instance
 * @param completed_state The state that just completed
 * @param user_data User data passed to callback
 */
typedef void (*TimerSessionCompleteCallback)(Timer *timer, TimerState completed_state, gpointer user_data);

/**
 * Creates a new timer instance
 * @return New Timer object
 */
Timer* timer_new(void);

/**
 * Frees a timer instance
 * @param timer Timer instance to free
 */
void timer_free(Timer *timer);

/**
 * Sets the timer durations in minutes
 * @param timer Timer instance
 * @param work_duration Work session duration in minutes
 * @param short_break_duration Short break duration in minutes
 * @param long_break_duration Long break duration in minutes
 * @param sessions_until_long Number of work sessions before long break
 */
void timer_set_durations(Timer *timer, int work_duration, int short_break_duration, 
                        int long_break_duration, int sessions_until_long);

/**
 * Sets callbacks for timer events
 * @param timer Timer instance
 * @param state_cb Callback for state changes (can be NULL)
 * @param tick_cb Callback for timer updates (can be NULL)
 * @param session_complete_cb Callback for session completion (can be NULL)
 * @param user_data User data passed to callbacks
 */
void timer_set_callbacks(Timer *timer, TimerStateCallback state_cb, 
                        TimerTickCallback tick_cb, TimerSessionCompleteCallback session_complete_cb,
                        gpointer user_data);

/**
 * Starts the timer
 * @param timer Timer instance
 */
void timer_start(Timer *timer);

/**
 * Pauses the timer
 * @param timer Timer instance
 */
void timer_pause(Timer *timer);

/**
 * Resets the timer to initial state
 * @param timer Timer instance
 */
void timer_reset(Timer *timer);

/**
 * Gets the current timer state
 * @param timer Timer instance
 * @return Current timer state
 */
TimerState timer_get_state(Timer *timer);

/**
 * Gets the current session number
 * @param timer Timer instance
 * @return Current session number (1-based)
 */
int timer_get_session(Timer *timer);

/**
 * Gets the remaining time
 * @param timer Timer instance
 * @param minutes Pointer to store minutes (can be NULL)
 * @param seconds Pointer to store seconds (can be NULL)
 */
void timer_get_remaining(Timer *timer, int *minutes, int *seconds);

/**
 * Gets the total duration for current timer state
 * @param timer Timer instance
 * @return Total duration in seconds
 */
int timer_get_total_duration(Timer *timer);

/**
 * Extends the current break by additional seconds
 * @param timer Timer instance
 * @param additional_seconds Seconds to add to current break
 */
void timer_extend_break(Timer *timer, int additional_seconds);

/**
 * Sets the auto-start work after break setting
 * @param timer Timer instance
 * @param auto_start TRUE to auto-start work after breaks, FALSE otherwise
 */
void timer_set_auto_start_work(Timer *timer, gboolean auto_start);

/**
 * Sets whether durations are in seconds (test mode) or minutes (normal mode)
 * @param timer Timer instance
 * @param use_seconds TRUE for seconds, FALSE for minutes
 */
void timer_set_duration_mode(Timer *timer, gboolean use_seconds);

/**
 * Skips the current work or break phase and starts the next one.
 * @param timer Timer instance
 */
void timer_skip_phase(Timer *timer);

G_END_DECLS

#endif // TIMER_H