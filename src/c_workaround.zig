// Temporary workaround for GTK/Zig C interop issues
// Known issue: GdkEvent contains opaque types which Zig cannot handle properly
// This prevents us from using delete-event and other event handlers directly
// See: https://github.com/ziglang/zig/issues/1499

const gtk = @import("gtk_minimal.zig");

// Re-export everything from gtk_minimal as 'c'
pub const c = gtk;

// Additional Cairo bindings we need (Cairo doesn't have the same issues)
pub const cairo = @cImport({
    @cInclude("cairo/cairo.h");
});

// Constants for clarity (re-export from gtk_minimal)
pub const TRUE = gtk.TRUE;
pub const FALSE = gtk.FALSE;

// External C functions from gtk_workaround.c
pub extern fn gtk_init_simple() void;
pub extern fn window_delete_handler(widget: *gtk.GtkWidget, event: ?*anyopaque, data: ?*anyopaque) c_int;