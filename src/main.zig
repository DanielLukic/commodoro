const std = @import("std");
const c = @cImport({
    @cInclude("gtk/gtk.h");
});
const ZigodoroApp = @import("app.zig").ZigodoroApp;
const dbus = @import("dbus/dbus.zig");

const CmdLineArgs = struct {
    work_duration: i32,           // in seconds for test mode, minutes for normal
    short_break_duration: i32,    
    long_break_duration: i32,     
    sessions_until_long_break: i32,
    test_mode: bool,              // TRUE if custom durations provided
};

fn parseDurationToSeconds(duration_str: []const u8) !i32 {
    if (duration_str.len == 0) return error.InvalidFormat;
    
    // Find where the number ends
    var num_end: usize = 0;
    for (duration_str, 0..) |char, i| {
        if (!std.ascii.isDigit(char)) {
            num_end = i;
            break;
        }
    } else {
        num_end = duration_str.len;
    }
    
    if (num_end == 0) return error.InvalidFormat;
    
    const value = try std.fmt.parseInt(i32, duration_str[0..num_end], 10);
    
    // Check for time unit suffix
    if (num_end < duration_str.len) {
        const suffix = duration_str[num_end];
        return switch (suffix) {
            's' => value,        // Seconds
            'm' => value * 60,   // Minutes
            'h' => value * 3600, // Hours
            else => error.InvalidFormat,
        };
    } else {
        // No suffix, assume minutes
        return value * 60;
    }
}

fn parseCommandLine(allocator: std.mem.Allocator) !CmdLineArgs {
    var args = CmdLineArgs{
        .work_duration = 25,
        .short_break_duration = 5,
        .long_break_duration = 15,
        .sessions_until_long_break = 4,
        .test_mode = false,
    };
    
    const argv = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, argv);
    
    if (argv.len >= 2) {
        args.work_duration = try parseDurationToSeconds(argv[1]);
        args.test_mode = true;
        std.debug.print("🧪 TEST MODE ACTIVE\n", .{});
        std.debug.print("Work: {s} ({d} seconds)\n", .{ argv[1], args.work_duration });
    }
    
    if (argv.len >= 3) {
        args.short_break_duration = try parseDurationToSeconds(argv[2]);
        std.debug.print("Short break: {s} ({d} seconds)\n", .{ argv[2], args.short_break_duration });
    }
    
    if (argv.len >= 4) {
        args.sessions_until_long_break = try std.fmt.parseInt(i32, argv[3], 10);
        std.debug.print("Sessions until long break: {d}\n", .{args.sessions_until_long_break});
    }
    
    if (argv.len >= 5) {
        args.long_break_duration = try parseDurationToSeconds(argv[4]);
        std.debug.print("Long break: {s} ({d} seconds)\n", .{ argv[4], args.long_break_duration });
    }
    
    if (args.test_mode) {
        std.debug.print("\n", .{});
    } else {
        std.debug.print("Normal mode - using default durations (25m/5m/15m, 4 sessions)\n", .{});
    }
    
    return args;
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    // Get command line arguments
    const argv = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, argv);
    
    // Check for D-Bus commands first
    var auto_start = false;
    var dbus_command: ?[]const u8 = null;
    
    for (argv[1..]) |arg| {
        if (std.mem.eql(u8, arg, "--help") or std.mem.eql(u8, arg, "-h")) {
            printUsage(argv[0]);
            return;
        } else if (std.mem.eql(u8, arg, "--auto-start")) {
            auto_start = true;
        } else {
            // Check if it's a D-Bus command
            if (dbus.parseCommand(arg)) |cmd| {
                dbus_command = cmd;
            }
        }
    }
    
    // Handle D-Bus commands
    if (dbus_command) |cmd| {
        const result = try dbus.sendCommand(cmd, auto_start);
        
        switch (result) {
            .success => return,
            .start_needed => {
                std.debug.print("Starting Zigodoro...\n", .{});
                // Continue to start the app
            },
            .not_running => {
                std.debug.print("Zigodoro is not running. Use --auto-start to launch it.\n", .{});
                std.process.exit(1);
            },
            .err => {
                std.debug.print("D-Bus error occurred\n", .{});
                std.process.exit(1);
            },
        }
    }

    // Parse command line arguments for timer mode
    const cmd_args = parseCommandLine(allocator) catch |err| {
        if (err == error.InvalidFormat) {
            std.debug.print("Invalid duration format. Use: 10s, 5m, 1h\n", .{});
            return;
        }
        return err;
    };

    // Initialize GTK
    c.gtk_init(null, null);

    // Create and run app
    const app = try ZigodoroApp.init(allocator, cmd_args);
    defer app.deinit();
    
    std.debug.print("Zigodoro started!\n", .{});
    
    app.run();
}

fn printUsage(program_name: []const u8) void {
    std.debug.print("Usage: {s} [work_duration] [short_break_duration] [sessions_until_long] [long_break_duration]\n", .{program_name});
    std.debug.print("       {s} <command> [--auto-start]\n\n", .{program_name});
    std.debug.print("Timer mode examples:\n", .{});
    std.debug.print("  {s}                    # Normal mode (25m work, 5m break)\n", .{program_name});
    std.debug.print("  {s} 15s 5s 4 10s       # Test mode (15s work, 5s break, 4 cycles, 10s long break)\n", .{program_name});
    std.debug.print("  {s} 2m 30s 2 1m        # Quick test (2m work, 30s break, 2 cycles, 1m long break)\n", .{program_name});
    std.debug.print("  {s} 45m 10m 3 20m      # Extended mode (45m work, 10m break, 3 cycles, 20m long break)\n\n", .{program_name});
    std.debug.print("Time units:\n", .{});
    std.debug.print("  s = seconds, m = minutes, h = hours\n", .{});
    std.debug.print("  No suffix defaults to minutes\n\n", .{});
    std.debug.print("D-Bus commands:\n", .{});
    std.debug.print("  toggle_timer          # Start/pause/resume the timer\n", .{});
    std.debug.print("  reset_timer           # Reset the timer\n", .{});
    std.debug.print("  toggle_break          # Skip to next phase\n", .{});
    std.debug.print("  show_hide             # Toggle window visibility\n", .{});
    std.debug.print("  --auto-start          # Start Zigodoro if not running\n\n", .{});
}