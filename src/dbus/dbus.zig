const std = @import("std");

// External C functions
extern fn dbus_send_command(command: [*:0]const u8, auto_start: c_int, unused: ?*anyopaque) c_int;
extern fn dbus_parse_command(str: [*:0]const u8) ?[*:0]const u8;

pub const DBusCommandResult = enum(c_int) {
    success = 0,
    not_running = 1,
    start_needed = 2,
    err = 3,
};

pub fn sendCommand(command: []const u8, auto_start: bool) !DBusCommandResult {
    var cmd_buf: [256]u8 = undefined;
    const cmd_z = try std.fmt.bufPrintZ(&cmd_buf, "{s}", .{command});
    
    const result = dbus_send_command(cmd_z, if (auto_start) 1 else 0, null);
    return @enumFromInt(result);
}

pub fn parseCommand(str: []const u8) ?[]const u8 {
    var str_buf: [256]u8 = undefined;
    const str_z = std.fmt.bufPrintZ(&str_buf, "{s}", .{str}) catch return null;
    
    const result = dbus_parse_command(str_z);
    if (result) |cmd| {
        return std.mem.span(cmd);
    }
    return null;
}