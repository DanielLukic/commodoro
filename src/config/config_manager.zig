const std = @import("std");
const Settings = @import("settings.zig").Settings;

pub const ConfigManager = struct {
    allocator: std.mem.Allocator,
    config_dir: []u8,
    config_file: []u8,
    state_file: []u8,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator) !*Self {
        const manager = try allocator.create(Self);
        errdefer allocator.destroy(manager);
        
        // Get home directory
        const home = std.process.getEnvVarOwned(allocator, "HOME") catch {
            return error.NoHomeDirectory;
        };
        defer allocator.free(home);
        
        // Create config directory path
        const config_dir = try std.fmt.allocPrint(allocator, "{s}/.config/zigodoro", .{home});
        errdefer allocator.free(config_dir);
        
        // Create config and state file paths
        const config_file = try std.fmt.allocPrint(allocator, "{s}/config.json", .{config_dir});
        errdefer allocator.free(config_file);
        
        const state_file = try std.fmt.allocPrint(allocator, "{s}/state.json", .{config_dir});
        errdefer allocator.free(state_file);
        
        manager.* = .{
            .allocator = allocator,
            .config_dir = config_dir,
            .config_file = config_file,
            .state_file = state_file,
        };
        
        // Ensure config directory exists
        try manager.ensureConfigDir();
        
        return manager;
    }
    
    pub fn deinit(self: *Self) void {
        self.allocator.free(self.config_dir);
        self.allocator.free(self.config_file);
        self.allocator.free(self.state_file);
        self.allocator.destroy(self);
    }
    
    fn ensureConfigDir(self: *Self) !void {
        std.fs.makeDirAbsolute(self.config_dir) catch |err| {
            switch (err) {
                error.PathAlreadyExists => {}, // OK, directory already exists
                else => return err,
            }
        };
    }
    
    pub fn loadSettings(self: *Self) !Settings {
        const file = std.fs.openFileAbsolute(self.config_file, .{}) catch |err| {
            switch (err) {
                error.FileNotFound => {
                    // Return default settings if config doesn't exist
                    return Settings.init(self.allocator);
                },
                else => return err,
            }
        };
        defer file.close();
        
        const stat = try file.stat();
        const contents = try self.allocator.alloc(u8, stat.size);
        defer self.allocator.free(contents);
        
        _ = try file.read(contents);
        
        // Parse JSON
        const parsed = try std.json.parseFromSlice(SettingsJson, self.allocator, contents, .{});
        defer parsed.deinit();
        
        // Convert to Settings struct
        var settings = Settings.init(self.allocator);
        settings.work_duration = parsed.value.work_duration;
        settings.short_break_duration = parsed.value.short_break_duration;
        settings.long_break_duration = parsed.value.long_break_duration;
        settings.sessions_until_long_break = parsed.value.sessions_until_long_break;
        settings.auto_start_work_after_break = parsed.value.auto_start_work_after_break;
        settings.enable_idle_detection = parsed.value.enable_idle_detection;
        settings.idle_timeout_minutes = parsed.value.idle_timeout_minutes;
        settings.enable_sounds = parsed.value.enable_sounds;
        settings.sound_volume = parsed.value.sound_volume;
        
        return settings;
    }
    
    pub fn saveSettings(self: *Self, settings: *const Settings) !void {
        // Create JSON object
        const json_settings = SettingsJson{
            .work_duration = settings.work_duration,
            .short_break_duration = settings.short_break_duration,
            .long_break_duration = settings.long_break_duration,
            .sessions_until_long_break = settings.sessions_until_long_break,
            .auto_start_work_after_break = settings.auto_start_work_after_break,
            .enable_idle_detection = settings.enable_idle_detection,
            .idle_timeout_minutes = settings.idle_timeout_minutes,
            .enable_sounds = settings.enable_sounds,
            .sound_volume = settings.sound_volume,
        };
        
        // Serialize to JSON
        var buffer = std.ArrayList(u8).init(self.allocator);
        defer buffer.deinit();
        
        try std.json.stringify(json_settings, .{ .emit_null_optional_fields = false }, buffer.writer());
        
        // Write to file
        const file = try std.fs.createFileAbsolute(self.config_file, .{});
        defer file.close();
        
        try file.writeAll(buffer.items);
    }
    
    pub fn loadState(self: *Self) !TimerState {
        const file = std.fs.openFileAbsolute(self.state_file, .{}) catch |err| {
            switch (err) {
                error.FileNotFound => {
                    // Return default state if file doesn't exist
                    return TimerState{
                        .session_count = 1,
                        .last_save_time = 0,
                    };
                },
                else => return err,
            }
        };
        defer file.close();
        
        const stat = try file.stat();
        const contents = try self.allocator.alloc(u8, stat.size);
        defer self.allocator.free(contents);
        
        _ = try file.read(contents);
        
        // Parse JSON
        const parsed = try std.json.parseFromSlice(TimerState, self.allocator, contents, .{});
        defer parsed.deinit();
        
        return parsed.value;
    }
    
    pub fn saveState(self: *Self, state: *const TimerState) !void {
        // Serialize to JSON
        var buffer = std.ArrayList(u8).init(self.allocator);
        defer buffer.deinit();
        
        try std.json.stringify(state, .{}, buffer.writer());
        
        // Write to file
        const file = try std.fs.createFileAbsolute(self.state_file, .{});
        defer file.close();
        
        try file.writeAll(buffer.items);
    }
};

// JSON structure for settings (subset of full Settings)
const SettingsJson = struct {
    work_duration: i32,
    short_break_duration: i32,
    long_break_duration: i32,
    sessions_until_long_break: i32,
    auto_start_work_after_break: bool,
    enable_idle_detection: bool,
    idle_timeout_minutes: i32,
    enable_sounds: bool,
    sound_volume: f64,
};

// Timer state for persistence
pub const TimerState = struct {
    session_count: i32,
    last_save_time: i64,
};