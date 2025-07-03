const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "zigodoro",
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Add C workaround file
    exe.addCSourceFile(.{
        .file = b.path("src/gtk_workaround.c"),
        .flags = &.{"-std=c99"},
    });
    
    // Add input monitor C file (needs GNU extensions for usleep)
    exe.addCSourceFile(.{
        .file = b.path("src/input/input_monitor.c"),
        .flags = &.{"-std=gnu99", "-I", "src"},
    });
    
    // Add tray icon C file
    exe.addCSourceFile(.{
        .file = b.path("src/gui/tray_icon.c"),
        .flags = &.{"-std=c99"},
    });
    
    // Add D-Bus C files
    exe.addCSourceFile(.{
        .file = b.path("src/dbus/dbus.c"),
        .flags = &.{"-std=c99"},
    });
    
    exe.addCSourceFile(.{
        .file = b.path("src/dbus/dbus_service.c"),
        .flags = &.{"-std=c99"},
    });

    // Link system libraries
    exe.linkSystemLibrary("gtk+-3.0");
    exe.linkSystemLibrary("glib-2.0");
    exe.linkSystemLibrary("gobject-2.0");
    exe.linkSystemLibrary("gio-2.0");
    exe.linkSystemLibrary("cairo");
    exe.linkSystemLibrary("gdk-pixbuf-2.0");
    exe.linkSystemLibrary("X11");
    exe.linkSystemLibrary("Xtst");
    exe.linkSystemLibrary("Xi");
    exe.linkSystemLibrary("Xss");
    exe.linkSystemLibrary("asound");
    exe.linkSystemLibrary("pthread");
    exe.linkSystemLibrary("m");
    exe.linkSystemLibrary("c");

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}