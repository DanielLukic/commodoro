const std = @import("std");
const c = @cImport({
    @cInclude("gtk/gtk.h");
});

// External C functions
extern fn input_monitor_new() ?*anyopaque;
extern fn input_monitor_free(monitor: *anyopaque) void;
extern fn input_monitor_set_callback(monitor: *anyopaque, callback: *const fn (c.gpointer) callconv(.C) c.gboolean, user_data: c.gpointer) void;
extern fn input_monitor_set_window(monitor: *anyopaque, window: *anyopaque) void;
extern fn input_monitor_start(monitor: *anyopaque) void;
extern fn input_monitor_stop(monitor: *anyopaque) void;
extern fn input_monitor_is_active(monitor: *anyopaque) c.gboolean;
extern fn input_monitor_get_idle_time(monitor: *anyopaque) c_int;

pub const InputMonitor = struct {
    monitor: *anyopaque,
    allocator: std.mem.Allocator,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator) !*Self {
        const self = try allocator.create(Self);
        errdefer allocator.destroy(self);
        
        const monitor = input_monitor_new() orelse return error.MonitorCreationFailed;
        
        self.* = .{
            .allocator = allocator,
            .monitor = monitor,
        };
        
        return self;
    }
    
    pub fn deinit(self: *Self) void {
        input_monitor_free(self.monitor);
        self.allocator.destroy(self);
    }
    
    pub fn setCallback(self: *Self, callback: *const fn (c.gpointer) callconv(.C) c.gboolean, user_data: ?*anyopaque) void {
        input_monitor_set_callback(self.monitor, callback, user_data);
    }
    
    pub fn setWindow(self: *Self, window: *anyopaque) void {
        input_monitor_set_window(self.monitor, window);
    }
    
    pub fn start(self: *Self) void {
        input_monitor_start(self.monitor);
    }
    
    pub fn stop(self: *Self) void {
        input_monitor_stop(self.monitor);
    }
    
    pub fn isActive(self: *Self) bool {
        return input_monitor_is_active(self.monitor) != 0;
    }
    
    pub fn getIdleTime(self: *Self) i32 {
        return input_monitor_get_idle_time(self.monitor);
    }
};