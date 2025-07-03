const std = @import("std");

pub const Settings = struct {
    // Timer settings
    work_duration: i32 = 25,              // minutes (1-120)
    short_break_duration: i32 = 5,        // minutes (1-60)
    long_break_duration: i32 = 15,        // minutes (5-120)
    sessions_until_long_break: i32 = 4,   // count (2-10)
    
    // Behavior settings
    auto_start_work_after_break: bool = true,
    enable_idle_detection: bool = false,
    idle_timeout_minutes: i32 = 5,        // minutes (1-30)
    
    // Audio settings
    enable_sounds: bool = true,
    sound_volume: f64 = 0.5,              // 0.0-1.0
    sound_type: []const u8 = "chimes",    // "chimes" or "custom"
    work_start_sound: ?[]const u8 = null,         // file path or null
    break_start_sound: ?[]const u8 = null,        // file path or null
    session_complete_sound: ?[]const u8 = null,   // file path or null
    timer_finish_sound: ?[]const u8 = null,       // file path or null
    
    allocator: std.mem.Allocator,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator) Self {
        return .{
            .allocator = allocator,
        };
    }
    
    pub fn deinit(self: *Self) void {
        if (self.work_start_sound) |sound| {
            self.allocator.free(sound);
        }
        if (self.break_start_sound) |sound| {
            self.allocator.free(sound);
        }
        if (self.session_complete_sound) |sound| {
            self.allocator.free(sound);
        }
        if (self.timer_finish_sound) |sound| {
            self.allocator.free(sound);
        }
    }
    
    pub fn copy(self: *const Self, allocator: std.mem.Allocator) !Self {
        var new_settings = Self.init(allocator);
        new_settings.work_duration = self.work_duration;
        new_settings.short_break_duration = self.short_break_duration;
        new_settings.long_break_duration = self.long_break_duration;
        new_settings.sessions_until_long_break = self.sessions_until_long_break;
        new_settings.auto_start_work_after_break = self.auto_start_work_after_break;
        new_settings.enable_idle_detection = self.enable_idle_detection;
        new_settings.idle_timeout_minutes = self.idle_timeout_minutes;
        new_settings.enable_sounds = self.enable_sounds;
        new_settings.sound_volume = self.sound_volume;
        new_settings.sound_type = self.sound_type;
        
        // Copy sound paths if they exist
        if (self.work_start_sound) |sound| {
            new_settings.work_start_sound = try allocator.dupe(u8, sound);
        }
        if (self.break_start_sound) |sound| {
            new_settings.break_start_sound = try allocator.dupe(u8, sound);
        }
        if (self.session_complete_sound) |sound| {
            new_settings.session_complete_sound = try allocator.dupe(u8, sound);
        }
        if (self.timer_finish_sound) |sound| {
            new_settings.timer_finish_sound = try allocator.dupe(u8, sound);
        }
        
        return new_settings;
    }
};