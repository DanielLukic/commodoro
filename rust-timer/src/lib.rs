use std::ffi::c_void;
use std::sync::Mutex;
use libc::c_int;

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum TimerState {
    Idle = 0,
    Work = 1,
    ShortBreak = 2,
    LongBreak = 3,
    Paused = 4,
}

// Callback type definitions
pub type TimerStateCallback = Option<extern "C" fn(*mut Timer, TimerState, *mut c_void)>;
pub type TimerTickCallback = Option<extern "C" fn(*mut Timer, i32, i32, *mut c_void)>;
pub type TimerSessionCompleteCallback = Option<extern "C" fn(*mut Timer, TimerState, *mut c_void)>;

pub struct Timer {
    state: TimerState,
    previous_state: TimerState,
    session_count: i32,
    
    // Durations in minutes (or seconds in test mode)
    work_duration: i32,
    short_break_duration: i32,
    long_break_duration: i32,
    sessions_until_long: i32,
    
    // Current timer state
    remaining_seconds: i32,
    total_seconds: i32,
    
    // Settings
    auto_start_work_after_break: bool,
    use_seconds_mode: bool,
    
    // Track what should start next when in IDLE
    work_session_just_finished: bool,
    
    // Callbacks
    state_callback: TimerStateCallback,
    tick_callback: TimerTickCallback,
    session_complete_callback: TimerSessionCompleteCallback,
    user_data: *mut c_void,
    
    // GLib timer ID
    timer_id: u32,
}

// We'll store a simple pointer as usize to avoid Send/Sync issues
static TIMER_INSTANCE: Mutex<Option<usize>> = Mutex::new(None);

// External GLib function
extern "C" {
    fn g_timeout_add(interval: u32, function: extern "C" fn(*mut c_void) -> c_int, data: *mut c_void) -> u32;
    fn g_source_remove(source_id: u32) -> c_int;
}

impl Timer {
    fn new() -> Self {
        let mut timer = Timer {
            state: TimerState::Idle,
            previous_state: TimerState::Idle,
            session_count: 1,
            work_duration: 25,
            short_break_duration: 5,
            long_break_duration: 15,
            sessions_until_long: 4,
            remaining_seconds: 0,
            total_seconds: 0,
            auto_start_work_after_break: true,
            use_seconds_mode: false,
            work_session_just_finished: false,
            state_callback: None,
            tick_callback: None,
            session_complete_callback: None,
            user_data: std::ptr::null_mut(),
            timer_id: 0,
        };
        
        // Set initial time to work duration
        timer.remaining_seconds = timer.get_duration_for_state(TimerState::Work);
        timer.total_seconds = timer.remaining_seconds;
        
        timer
    }
    
    fn get_duration_for_state(&self, state: TimerState) -> i32 {
        let duration = match state {
            TimerState::Work => self.work_duration,
            TimerState::ShortBreak => self.short_break_duration,
            TimerState::LongBreak => self.long_break_duration,
            _ => 0,
        };
        
        if self.use_seconds_mode {
            duration
        } else {
            duration * 60
        }
    }
    
    fn set_state(&mut self, new_state: TimerState) {
        if self.state == new_state {
            return;
        }
        
        self.state = new_state;
        
        if let Some(callback) = self.state_callback {
            callback(self as *mut Timer, new_state, self.user_data);
        }
    }
    
    fn transition_to_next_state(&mut self) {
        match self.state {
            TimerState::Work => {
                self.work_session_just_finished = true;
                
                if self.session_count % self.sessions_until_long == 0 {
                    // Set duration BEFORE changing state to avoid 00:00 display
                    self.remaining_seconds = self.get_duration_for_state(TimerState::LongBreak);
                    self.total_seconds = self.remaining_seconds;
                    self.set_state(TimerState::LongBreak);
                } else {
                    // Set duration BEFORE changing state to avoid 00:00 display
                    self.remaining_seconds = self.get_duration_for_state(TimerState::ShortBreak);
                    self.total_seconds = self.remaining_seconds;
                    self.set_state(TimerState::ShortBreak);
                }
                
                self.session_count += 1;
            }
            TimerState::ShortBreak | TimerState::LongBreak => {
                // Always transition to idle first, let the app handle auto-start via input monitoring
                self.remaining_seconds = self.get_duration_for_state(TimerState::Work);
                self.total_seconds = self.remaining_seconds;
                self.set_state(TimerState::Idle);
            }
            _ => {}
        }
    }
    
    fn tick(&mut self) {
        if self.state == TimerState::Idle || self.state == TimerState::Paused {
            return;
        }
        
        self.remaining_seconds -= 1;
        
        if self.remaining_seconds < 0 {
            self.remaining_seconds = 0;
        }
        
        let minutes = self.remaining_seconds / 60;
        let seconds = self.remaining_seconds % 60;
        
        if let Some(callback) = self.tick_callback {
            callback(self as *mut Timer, minutes, seconds, self.user_data);
        }
        
        if self.remaining_seconds == 0 {
            let completed_state = self.state;
            
            if let Some(callback) = self.session_complete_callback {
                callback(self as *mut Timer, completed_state, self.user_data);
            }
            
            self.transition_to_next_state();
        }
    }
}

// C FFI functions
#[no_mangle]
pub extern "C" fn timer_new() -> *mut Timer {
    let timer = Box::new(Timer::new());
    Box::into_raw(timer)
}

#[no_mangle]
pub extern "C" fn timer_free(timer: *mut Timer) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        let timer_ref = &*timer;
        
        // Remove GLib timer if active
        if timer_ref.timer_id > 0 {
            g_source_remove(timer_ref.timer_id);
        }
        
        // Remove from global instance if it's there
        if let Ok(mut global) = TIMER_INSTANCE.lock() {
            if *global == Some(timer as usize) {
                *global = None;
            }
        }
        
        drop(Box::from_raw(timer));
    }
}

#[no_mangle]
pub extern "C" fn timer_set_durations(
    timer: *mut Timer,
    work_duration: i32,
    short_break_duration: i32,
    long_break_duration: i32,
    sessions_until_long: i32,
) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        let timer = &mut *timer;
        timer.work_duration = work_duration;
        timer.short_break_duration = short_break_duration;
        timer.long_break_duration = long_break_duration;
        timer.sessions_until_long = sessions_until_long;
    }
}

#[no_mangle]
pub extern "C" fn timer_set_callbacks(
    timer: *mut Timer,
    state_cb: TimerStateCallback,
    tick_cb: TimerTickCallback,
    session_complete_cb: TimerSessionCompleteCallback,
    user_data: *mut c_void,
) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        let timer = &mut *timer;
        timer.state_callback = state_cb;
        timer.tick_callback = tick_cb;
        timer.session_complete_callback = session_complete_cb;
        timer.user_data = user_data;
    }
}

#[no_mangle]
pub extern "C" fn timer_start(timer: *mut Timer) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        let timer = &mut *timer;
        
        match timer.state {
            TimerState::Idle => {
                timer.set_state(TimerState::Work);
                timer.remaining_seconds = timer.get_duration_for_state(TimerState::Work);
                timer.total_seconds = timer.remaining_seconds;
                timer.work_session_just_finished = false;
            }
            TimerState::Paused => {
                timer.set_state(timer.previous_state);
            }
            _ => {}
        }
        
        // Store timer instance for tick function
        if let Ok(mut global) = TIMER_INSTANCE.lock() {
            *global = Some(timer as *mut Timer as usize);
        }
        
        // Start GLib timer if not already running
        if timer.timer_id == 0 {
            timer.timer_id = g_timeout_add(1000, timer_tick_callback, timer as *mut Timer as *mut c_void);
        }
    }
}

#[no_mangle]
pub extern "C" fn timer_pause(timer: *mut Timer) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        let timer = &mut *timer;
        
        if timer.state != TimerState::Idle && timer.state != TimerState::Paused {
            timer.previous_state = timer.state;
            timer.set_state(TimerState::Paused);
            
            // Stop GLib timer
            if timer.timer_id > 0 {
                g_source_remove(timer.timer_id);
                timer.timer_id = 0;
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn timer_reset(timer: *mut Timer) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        let timer = &mut *timer;
        
        timer.set_state(TimerState::Idle);
        timer.session_count = 1;
        timer.remaining_seconds = timer.get_duration_for_state(TimerState::Work);
        timer.total_seconds = timer.remaining_seconds;
        timer.work_session_just_finished = false;
        
        // Stop GLib timer
        if timer.timer_id > 0 {
            g_source_remove(timer.timer_id);
            timer.timer_id = 0;
        }
    }
}

#[no_mangle]
pub extern "C" fn timer_get_state(timer: *mut Timer) -> TimerState {
    if timer.is_null() {
        return TimerState::Idle;
    }
    
    unsafe { (*timer).state }
}

#[no_mangle]
pub extern "C" fn timer_get_session(timer: *mut Timer) -> i32 {
    if timer.is_null() {
        return 0;
    }
    
    unsafe { (*timer).session_count }
}

#[no_mangle]
pub extern "C" fn timer_get_remaining(timer: *mut Timer, minutes: *mut i32, seconds: *mut i32) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        let timer = &*timer;
        let mins = timer.remaining_seconds / 60;
        let secs = timer.remaining_seconds % 60;
        
        if !minutes.is_null() {
            *minutes = mins;
        }
        
        if !seconds.is_null() {
            *seconds = secs;
        }
    }
}

#[no_mangle]
pub extern "C" fn timer_get_total_duration(timer: *mut Timer) -> i32 {
    if timer.is_null() {
        return 0;
    }
    
    unsafe { (*timer).total_seconds }
}

#[no_mangle]
pub extern "C" fn timer_extend_break(timer: *mut Timer, additional_seconds: i32) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        let timer = &mut *timer;
        
        if timer.state == TimerState::ShortBreak || timer.state == TimerState::LongBreak {
            timer.remaining_seconds += additional_seconds;
            timer.total_seconds += additional_seconds;
        }
    }
}

#[no_mangle]
pub extern "C" fn timer_set_auto_start_work(timer: *mut Timer, auto_start: i32) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        (*timer).auto_start_work_after_break = auto_start != 0;
    }
}

#[no_mangle]
pub extern "C" fn timer_set_duration_mode(timer: *mut Timer, use_seconds: i32) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        (*timer).use_seconds_mode = use_seconds != 0;
    }
}

#[no_mangle]
pub extern "C" fn timer_skip_phase(timer: *mut Timer) {
    if timer.is_null() {
        return;
    }
    
    unsafe {
        let timer = &mut *timer;
        
        if timer.state == TimerState::Work || 
           timer.state == TimerState::ShortBreak || 
           timer.state == TimerState::LongBreak {
            timer.remaining_seconds = 0;
            timer.tick();
        }
    }
}

// Timer tick function to be called by GLib
#[no_mangle]
pub extern "C" fn timer_tick_callback(user_data: *mut c_void) -> c_int {
    if user_data.is_null() {
        return 0;
    }
    
    unsafe {
        let timer = &mut *(user_data as *mut Timer);
        
        // Simple tick every second - GLib handles the timing
        timer.tick();
        
        1 // Continue timer
    }
}