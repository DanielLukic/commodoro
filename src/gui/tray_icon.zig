const std = @import("std");
const TimerState = @import("../timer/timer.zig").TimerState;

// External C functions
extern fn tray_icon_new() ?*anyopaque;
extern fn tray_icon_free(icon: *anyopaque) void;
extern fn tray_icon_update(icon: *anyopaque, state: c_int, current_seconds: c_int, total_seconds: c_int) void;
extern fn tray_icon_set_tooltip(icon: *anyopaque, tooltip: [*:0]const u8) void;
extern fn tray_icon_get_surface(icon: *anyopaque) ?*anyopaque;

pub const TrayIcon = struct {
    allocator: std.mem.Allocator,
    icon: *anyopaque,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator) !*Self {
        const self = try allocator.create(Self);
        errdefer allocator.destroy(self);
        
        const icon = tray_icon_new() orelse return error.TrayIconCreationFailed;
        
        self.* = .{
            .allocator = allocator,
            .icon = icon,
        };
        return self;
    }
    
    pub fn deinit(self: *Self) void {
        tray_icon_free(self.icon);
        self.allocator.destroy(self);
    }
    
    pub fn update(self: *Self, state: TimerState, remaining_seconds: i32, total_seconds: i32) !void {
        // Convert TimerState to TrayIconState
        const tray_state: c_int = switch (state) {
            .idle => 0,      // TRAY_STATE_IDLE
            .work => 1,      // TRAY_STATE_WORK
            .short_break => 2, // TRAY_STATE_SHORT_BREAK
            .long_break => 3,  // TRAY_STATE_LONG_BREAK
            .paused => 4,      // TRAY_STATE_PAUSED
        };
        
        tray_icon_update(self.icon, tray_state, remaining_seconds, total_seconds);
    }
    
    pub fn setTooltip(self: *Self, tooltip: []const u8) !void {
        var buf: [256]u8 = undefined;
        const tooltip_z = try std.fmt.bufPrintZ(&buf, "{s}", .{tooltip});
        tray_icon_set_tooltip(self.icon, tooltip_z);
    }
    
    pub fn getSurface(self: *Self) ?*anyopaque {
        return tray_icon_get_surface(self.icon);
    }
};