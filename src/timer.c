#include "timer.h"

struct _Timer {
    TimerState state;
    TimerState previous_state; // State before pausing
    int session_count;
    
    // Durations in minutes
    int work_duration;
    int short_break_duration;
    int long_break_duration;
    int sessions_until_long;
    
    // Current timer state
    int remaining_seconds;
    int total_seconds;
    guint timer_id;
    
    // Settings
    gboolean auto_start_work_after_break;
    gboolean use_seconds_mode;  // TRUE for test mode (durations in seconds), FALSE for normal (minutes)
    
    // Track what should start next when in IDLE
    gboolean work_session_just_finished;
    
    // Callbacks
    TimerStateCallback state_callback;
    TimerTickCallback tick_callback;
    TimerSessionCompleteCallback session_complete_callback;
    gpointer user_data;
};

static gboolean timer_tick_internal(gpointer user_data);
static void timer_transition_to_next_state(Timer *timer);
static void timer_set_state(Timer *timer, TimerState new_state);
static int timer_get_duration_for_state(Timer *timer, TimerState state);

Timer* timer_new(void) {
    Timer *timer = g_malloc0(sizeof(Timer));
    
    // Set default durations (25/5/15 minute Pomodoro)
    timer->work_duration = 25;
    timer->short_break_duration = 5;
    timer->long_break_duration = 15;
    timer->sessions_until_long = 4;
    
    timer->state = TIMER_STATE_IDLE;
    timer->previous_state = TIMER_STATE_IDLE;
    timer->session_count = 1;
    timer->timer_id = 0;
    timer->work_session_just_finished = FALSE;
    
    // Default settings
    timer->auto_start_work_after_break = TRUE;  // Default to auto-start
    timer->use_seconds_mode = FALSE;  // Default to minutes
    
    // Set initial time to work duration
    timer->remaining_seconds = timer_get_duration_for_state(timer, TIMER_STATE_WORK);
    timer->total_seconds = timer->remaining_seconds;
    
    return timer;
}

void timer_free(Timer *timer) {
    if (!timer) return;
    
    if (timer->timer_id > 0) {
        g_source_remove(timer->timer_id);
    }
    
    g_free(timer);
}

void timer_set_durations(Timer *timer, int work_duration, int short_break_duration,
                        int long_break_duration, int sessions_until_long) {
    if (!timer) return;
    
    timer->work_duration = work_duration;
    timer->short_break_duration = short_break_duration;
    timer->long_break_duration = long_break_duration;
    timer->sessions_until_long = sessions_until_long;
}

void timer_set_callbacks(Timer *timer, TimerStateCallback state_cb,
                        TimerTickCallback tick_cb, TimerSessionCompleteCallback session_complete_cb,
                        gpointer user_data) {
    if (!timer) return;
    
    timer->state_callback = state_cb;
    timer->tick_callback = tick_cb;
    timer->session_complete_callback = session_complete_cb;
    timer->user_data = user_data;
}

void timer_start(Timer *timer) {
    if (!timer) return;
    
    
    if (timer->state == TIMER_STATE_IDLE) {
        // Always start work session from IDLE
        timer->work_session_just_finished = FALSE;  // Clear flag
        timer_set_state(timer, TIMER_STATE_WORK);
    } else if (timer->state == TIMER_STATE_PAUSED) {
        // Resume from paused state - restore the previous state
        timer->state = timer->previous_state;
        
        // Call state callback to update UI
        if (timer->state_callback) {
            timer->state_callback(timer, timer->state, timer->user_data);
        }
    }
    
    if (timer->timer_id == 0) {
        timer->timer_id = g_timeout_add(1000, timer_tick_internal, timer);
    } else {
    }
}

void timer_pause(Timer *timer) {
    if (!timer) return;
    
    if (timer->timer_id > 0) {
        g_source_remove(timer->timer_id);
        timer->timer_id = 0;
    }
    
    if (timer->state != TIMER_STATE_IDLE && timer->state != TIMER_STATE_PAUSED) {
        // Save current state before pausing
        timer->previous_state = timer->state;
        timer->state = TIMER_STATE_PAUSED;
        
        // Call state callback to update UI
        if (timer->state_callback) {
            timer->state_callback(timer, TIMER_STATE_PAUSED, timer->user_data);
        }
    }
}

void timer_reset(Timer *timer) {
    if (!timer) return;
    
    if (timer->timer_id > 0) {
        g_source_remove(timer->timer_id);
        timer->timer_id = 0;
    }
    
    timer->session_count = 1;
    timer->previous_state = TIMER_STATE_IDLE;
    timer->work_session_just_finished = FALSE;  // Clear the flag
    timer_set_state(timer, TIMER_STATE_IDLE);
}

TimerState timer_get_state(Timer *timer) {
    if (!timer) return TIMER_STATE_IDLE;
    return timer->state;
}

int timer_get_session(Timer *timer) {
    if (!timer) return 1;
    return timer->session_count;
}

void timer_get_remaining(Timer *timer, int *minutes, int *seconds) {
    if (!timer) {
        if (minutes) *minutes = 0;
        if (seconds) *seconds = 0;
        return;
    }
    
    if (minutes) *minutes = timer->remaining_seconds / 60;
    if (seconds) *seconds = timer->remaining_seconds % 60;
}

int timer_get_total_duration(Timer *timer) {
    if (!timer) return 0;
    return timer->total_seconds;
}

static gboolean timer_tick_internal(gpointer user_data) {
    Timer *timer = (Timer*)user_data;
    
    
    if (timer->remaining_seconds > 0) {
        timer->remaining_seconds--;
        
        // Call tick callback
        if (timer->tick_callback) {
            int minutes = timer->remaining_seconds / 60;
            int seconds = timer->remaining_seconds % 60;
            timer->tick_callback(timer, minutes, seconds, timer->user_data);
        }
        
        return G_SOURCE_CONTINUE;
    } else {
        // Timer finished, transition to next state
        timer->timer_id = 0;
        timer_transition_to_next_state(timer);
        return G_SOURCE_REMOVE;
    }
}

static void timer_transition_to_next_state(Timer *timer) {
    gboolean should_auto_start = TRUE;
    
    switch (timer->state) {
        case TIMER_STATE_WORK:
            // Work session finished - signal completion then start appropriate break
            timer->session_count++;
            timer->work_session_just_finished = TRUE;
            
            // Signal work session completion (triggers session_complete sound)
            if (timer->session_complete_callback) {
                timer->session_complete_callback(timer, TIMER_STATE_WORK, timer->user_data);
            }
            
            // Determine which type of break to start
            TimerState break_type;
            if ((timer->session_count - 1) % timer->sessions_until_long == 0) {
                break_type = TIMER_STATE_LONG_BREAK;
            } else {
                break_type = TIMER_STATE_SHORT_BREAK;
            }
            
            timer_set_state(timer, break_type);
            should_auto_start = TRUE;  // Auto-start the break
            break;
            
        case TIMER_STATE_SHORT_BREAK:
        case TIMER_STATE_LONG_BREAK:
            // Break finished - signal completion and go to IDLE
            timer->work_session_just_finished = FALSE;  // Clear the flag
            
            // Signal break completion (triggers timer_finish sound)
            if (timer->session_complete_callback) {
                timer->session_complete_callback(timer, timer->state, timer->user_data);
            }
            
            timer_set_state(timer, TIMER_STATE_IDLE);
            should_auto_start = FALSE;  // Never auto-start after breaks
            break;
            
        case TIMER_STATE_IDLE:
        case TIMER_STATE_PAUSED:
            // Should not happen
            should_auto_start = FALSE;
            break;
    }
    
    // Auto-start the next phase if appropriate
    if (should_auto_start && timer->state != TIMER_STATE_IDLE && timer->state != TIMER_STATE_PAUSED) {
        timer->timer_id = g_timeout_add(1000, timer_tick_internal, timer);
    }
}

static void timer_set_state(Timer *timer, TimerState new_state) {
    timer->state = new_state;
    
    if (new_state == TIMER_STATE_IDLE) {
        timer->remaining_seconds = timer_get_duration_for_state(timer, TIMER_STATE_WORK);
        timer->total_seconds = timer->remaining_seconds;
    } else {
        timer->remaining_seconds = timer_get_duration_for_state(timer, new_state);
        timer->total_seconds = timer->remaining_seconds;
    }
    
    // Call state callback
    if (timer->state_callback) {
        timer->state_callback(timer, new_state, timer->user_data);
    }
    
    // Call tick callback to update display
    if (timer->tick_callback) {
        int minutes = timer->remaining_seconds / 60;
        int seconds = timer->remaining_seconds % 60;
        timer->tick_callback(timer, minutes, seconds, timer->user_data);
    }
}

void timer_extend_break(Timer *timer, int additional_seconds) {
    if (!timer) return;
    
    // Only extend during break states
    if (timer->state != TIMER_STATE_SHORT_BREAK && timer->state != TIMER_STATE_LONG_BREAK) {
        return;
    }
    
    // Add the additional seconds to remaining time
    timer->remaining_seconds += additional_seconds;
    
    // Update total duration to reflect the extension
    timer->total_seconds += additional_seconds;
    
    // Call tick callback to update display immediately
    if (timer->tick_callback) {
        int minutes = timer->remaining_seconds / 60;
        int seconds = timer->remaining_seconds % 60;
        timer->tick_callback(timer, minutes, seconds, timer->user_data);
    }
}

void timer_set_auto_start_work(Timer *timer, gboolean auto_start) {
    if (!timer) return;
    timer->auto_start_work_after_break = auto_start;
}

void timer_set_duration_mode(Timer *timer, gboolean use_seconds) {
    if (!timer) return;
    timer->use_seconds_mode = use_seconds;
}

static int timer_get_duration_for_state(Timer *timer, TimerState state) {
    int duration;
    switch (state) {
        case TIMER_STATE_WORK:
            duration = timer->work_duration;
            break;
        case TIMER_STATE_SHORT_BREAK:
            duration = timer->short_break_duration;
            break;
        case TIMER_STATE_LONG_BREAK:
            duration = timer->long_break_duration;
            break;
        case TIMER_STATE_IDLE:
        case TIMER_STATE_PAUSED:
        default:
            duration = timer->work_duration;
            break;
    }
    
    // Convert to seconds based on mode
    if (timer->use_seconds_mode) {
        return duration;  // Already in seconds
    } else {
        return duration * 60;  // Convert minutes to seconds
    }
}

void timer_skip_phase(Timer *timer) {
    if (!timer) return;

    if (timer->timer_id > 0) {
        g_source_remove(timer->timer_id);
        timer->timer_id = 0;
    }

    switch (timer->state) {
        case TIMER_STATE_WORK:
            timer_transition_to_next_state(timer);
            break;
        case TIMER_STATE_SHORT_BREAK:
        case TIMER_STATE_LONG_BREAK:
            timer->work_session_just_finished = FALSE;
            timer_set_state(timer, TIMER_STATE_WORK);
            if (timer->state != TIMER_STATE_IDLE && timer->state != TIMER_STATE_PAUSED) {
                timer->timer_id = g_timeout_add(1000, timer_tick_internal, timer);
            }
            break;
        case TIMER_STATE_IDLE:
        case TIMER_STATE_PAUSED:
            break;
    }
}
