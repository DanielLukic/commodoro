const std = @import("std");
const c = @cImport({
    @cInclude("glib.h");
});

pub const TimerState = enum(u8) {
    idle,
    work,
    short_break,
    long_break,
    paused,
};

pub const Timer = struct {
    state: TimerState,
    previous_state: TimerState, // State before pausing
    session_count: i32,
    
    // Durations in minutes
    work_duration: i32,
    short_break_duration: i32,
    long_break_duration: i32,
    sessions_until_long: i32,
    
    // Current timer state
    remaining_seconds: i32,
    total_seconds: i32,
    timer_id: c.guint,
    
    // Settings
    auto_start_work_after_break: bool,
    use_seconds_mode: bool, // TRUE for test mode (durations in seconds), FALSE for normal (minutes)
    
    // Track what should start next when in IDLE
    work_session_just_finished: bool,
    
    // Callbacks
    state_callback: ?*const fn (*Timer, TimerState, ?*anyopaque) void,
    tick_callback: ?*const fn (*Timer, i32, i32, ?*anyopaque) void,
    session_complete_callback: ?*const fn (*Timer, TimerState, ?*anyopaque) void,
    user_data: ?*anyopaque,
    
    // Allocator
    allocator: std.mem.Allocator,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator) !*Self {
        const timer = try allocator.create(Self);
        errdefer allocator.destroy(timer);
        
        timer.* = .{
            .allocator = allocator,
            .state = .idle,
            .previous_state = .idle,
            .session_count = 1,
            .work_duration = 25,
            .short_break_duration = 5,
            .long_break_duration = 15,
            .sessions_until_long = 4,
            .remaining_seconds = 0,
            .total_seconds = 0,
            .timer_id = 0,
            .work_session_just_finished = false,
            .auto_start_work_after_break = true,
            .use_seconds_mode = false,
            .state_callback = null,
            .tick_callback = null,
            .session_complete_callback = null,
            .user_data = null,
        };
        
        // Set initial time to work duration
        timer.remaining_seconds = timer.getDurationForState(.work);
        timer.total_seconds = timer.remaining_seconds;
        
        return timer;
    }
    
    pub fn deinit(self: *Self) void {
        if (self.timer_id > 0) {
            _ = c.g_source_remove(self.timer_id);
        }
        self.allocator.destroy(self);
    }
    
    pub fn setDurations(self: *Self, work: i32, short_break: i32, long_break: i32, sessions_until_long: i32) void {
        self.work_duration = work;
        self.short_break_duration = short_break;
        self.long_break_duration = long_break;
        self.sessions_until_long = sessions_until_long;
    }
    
    pub fn setCallbacks(
        self: *Self,
        state_cb: ?*const fn (*Timer, TimerState, ?*anyopaque) void,
        tick_cb: ?*const fn (*Timer, i32, i32, ?*anyopaque) void,
        session_complete_cb: ?*const fn (*Timer, TimerState, ?*anyopaque) void,
        user_data: ?*anyopaque,
    ) void {
        self.state_callback = state_cb;
        self.tick_callback = tick_cb;
        self.session_complete_callback = session_complete_cb;
        self.user_data = user_data;
    }
    
    pub fn start(self: *Self) void {
        if (self.state == .idle) {
            // Always start work session from IDLE
            self.work_session_just_finished = false;
            self.setState(.work);
        } else if (self.state == .paused) {
            // Resume from paused state - restore the previous state
            self.state = self.previous_state;
            
            // Call state callback to update UI
            if (self.state_callback) |cb| {
                cb(self, self.state, self.user_data);
            }
        }
        
        if (self.timer_id == 0) {
            self.timer_id = c.g_timeout_add(1000, timerTickInternal, self);
        }
    }
    
    pub fn pause(self: *Self) void {
        if (self.timer_id > 0) {
            _ = c.g_source_remove(self.timer_id);
            self.timer_id = 0;
        }
        
        if (self.state != .idle and self.state != .paused) {
            // Save current state before pausing
            self.previous_state = self.state;
            self.state = .paused;
            
            // Call state callback to update UI
            if (self.state_callback) |cb| {
                cb(self, .paused, self.user_data);
            }
        }
    }
    
    pub fn reset(self: *Self) void {
        if (self.timer_id > 0) {
            _ = c.g_source_remove(self.timer_id);
            self.timer_id = 0;
        }
        
        self.session_count = 1;
        self.previous_state = .idle;
        self.work_session_just_finished = false;
        self.setState(.idle);
    }
    
    pub fn getState(self: *const Self) TimerState {
        return self.state;
    }
    
    pub fn getSession(self: *const Self) i32 {
        return self.session_count;
    }
    
    pub fn getRemaining(self: *const Self) struct { minutes: i32, seconds: i32 } {
        return .{
            .minutes = @divTrunc(self.remaining_seconds, 60),
            .seconds = @mod(self.remaining_seconds, 60),
        };
    }
    
    pub fn getTotalDuration(self: *const Self) i32 {
        return self.total_seconds;
    }
    
    pub fn extendBreak(self: *Self, additional_seconds: i32) void {
        // Only extend during break states
        if (self.state != .short_break and self.state != .long_break) {
            return;
        }
        
        // Add the additional seconds to remaining time
        self.remaining_seconds += additional_seconds;
        
        // Update total duration to reflect the extension
        self.total_seconds += additional_seconds;
        
        // Call tick callback to update display immediately
        if (self.tick_callback) |cb| {
            const time = self.getRemaining();
            cb(self, time.minutes, time.seconds, self.user_data);
        }
    }
    
    pub fn setAutoStartWork(self: *Self, auto_start: bool) void {
        self.auto_start_work_after_break = auto_start;
    }
    
    pub fn setDurationMode(self: *Self, use_seconds: bool) void {
        self.use_seconds_mode = use_seconds;
    }
    
    fn setState(self: *Self, new_state: TimerState) void {
        self.state = new_state;
        
        if (new_state == .idle) {
            self.remaining_seconds = self.getDurationForState(.work);
            self.total_seconds = self.remaining_seconds;
        } else {
            self.remaining_seconds = self.getDurationForState(new_state);
            self.total_seconds = self.remaining_seconds;
        }
        
        // Call state callback
        if (self.state_callback) |cb| {
            cb(self, new_state, self.user_data);
        }
        
        // Call tick callback to update display
        if (self.tick_callback) |cb| {
            const time = self.getRemaining();
            cb(self, time.minutes, time.seconds, self.user_data);
        }
    }
    
    fn getDurationForState(self: *const Self, state: TimerState) i32 {
        const duration = switch (state) {
            .work => self.work_duration,
            .short_break => self.short_break_duration,
            .long_break => self.long_break_duration,
            .idle, .paused => self.work_duration,
        };
        
        // Convert to seconds based on mode
        if (self.use_seconds_mode) {
            return duration; // Already in seconds
        } else {
            return duration * 60; // Convert minutes to seconds
        }
    }
    
    fn transitionToNextState(self: *Self) void {
        var should_auto_start = true;
        
        switch (self.state) {
            .work => {
                // Work session finished - signal completion then start appropriate break
                self.session_count += 1;
                self.work_session_just_finished = true;
                
                // Signal work session completion (triggers session_complete sound)
                if (self.session_complete_callback) |cb| {
                    cb(self, .work, self.user_data);
                }
                
                // Determine which type of break to start
                const break_type: TimerState = if (@mod(self.session_count - 1, self.sessions_until_long) == 0)
                    .long_break
                else
                    .short_break;
                
                self.setState(break_type);
                should_auto_start = true; // Auto-start the break
            },
            .short_break, .long_break => {
                // Break finished - signal completion and go to IDLE
                self.work_session_just_finished = false;
                
                // Signal break completion (triggers timer_finish sound)
                if (self.session_complete_callback) |cb| {
                    cb(self, self.state, self.user_data);
                }
                
                self.setState(.idle);
                should_auto_start = false; // Never auto-start after breaks
            },
            .idle, .paused => {
                // Should not happen
                should_auto_start = false;
            },
        }
        
        // Auto-start the next phase if appropriate
        if (should_auto_start and self.state != .idle and self.state != .paused) {
            self.timer_id = c.g_timeout_add(1000, timerTickInternal, self);
        }
    }
    
    pub fn skipPhase(self: *Self) void {
        if (self.timer_id > 0) {
            _ = c.g_source_remove(self.timer_id);
            self.timer_id = 0;
        }

        switch (self.state) {
            .work => {
                self.transitionToNextState();
            },
            .short_break, .long_break => {
                self.work_session_just_finished = false;
                self.setState(.work);
                if (self.state != .idle and self.state != .paused) {
                    self.timer_id = c.g_timeout_add(1000, timerTickInternal, self);
                }
            },
            .idle, .paused => {
                // Do nothing for idle and paused states
            },
        }
    }
    
    fn timerTickInternal(user_data: c.gpointer) callconv(.C) c.gboolean {
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        if (self.remaining_seconds > 0) {
            self.remaining_seconds -= 1;
            
            // Call tick callback
            if (self.tick_callback) |cb| {
                const time = self.getRemaining();
                cb(self, time.minutes, time.seconds, self.user_data);
            }
            
            return @as(c.gboolean, 1); // G_SOURCE_CONTINUE
        } else {
            // Timer finished, transition to next state
            self.timer_id = 0;
            self.transitionToNextState();
            return @as(c.gboolean, 0); // G_SOURCE_REMOVE
        }
    }
};