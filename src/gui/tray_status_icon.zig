const std = @import("std");
// Use workaround to avoid GdkEvent opaque type issues
const cw = @import("../c_workaround.zig");
const c = cw.c;

pub const TrayStatusIcon = struct {
    status_icon: *c.GtkStatusIcon,
    callback: ?*const fn (action: []const u8, user_data: ?*anyopaque) void,
    user_data: ?*anyopaque,
    allocator: std.mem.Allocator,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator) !*Self {
        const tray = try allocator.create(Self);
        
        // Create GTK3 status icon
        const status_icon = c.gtk_status_icon_new() orelse return error.StatusIconCreationFailed;
        
        tray.* = .{
            .allocator = allocator,
            .status_icon = status_icon,
            .callback = null,
            .user_data = null,
        };
        
        // Set default properties
        c.gtk_status_icon_set_title(status_icon, "Zigodoro");
        c.gtk_status_icon_set_tooltip_text(status_icon, "Zigodoro Timer");
        c.gtk_status_icon_set_from_icon_name(status_icon, "appointment-soon");
        c.gtk_status_icon_set_visible(status_icon, 1);
        
        // Connect signals
        _ = c.g_signal_connect_data(status_icon, "activate", @ptrCast(&onTrayActivate), tray, null, 0);
        _ = c.g_signal_connect_data(status_icon, "popup-menu", @ptrCast(&onTrayPopupMenu), tray, null, 0);
        
        return tray;
    }
    
    pub fn deinit(self: *Self) void {
        c.g_object_unref(self.status_icon);
        self.allocator.destroy(self);
    }
    
    pub fn setCallback(self: *Self, callback: *const fn ([]const u8, ?*anyopaque) void, user_data: ?*anyopaque) void {
        self.callback = callback;
        self.user_data = user_data;
    }
    
    pub fn update(self: *Self, surface: ?*anyopaque, tooltip: []const u8) void {
        if (surface) |surf| {
            // Update the status icon with the Cairo surface
            const width = c.cairo_image_surface_get_width(surf);
            const height = c.cairo_image_surface_get_height(surf);
            
            std.log.info("Updating status icon with Cairo surface: {}x{}", .{width, height});
            
            const pixbuf = c.gdk_pixbuf_get_from_surface(surf, 0, 0, width, height);
            if (pixbuf) |pb| {
                c.gtk_status_icon_set_from_pixbuf(self.status_icon, pb);
                c.g_object_unref(pb);
            } else {
                std.log.err("Failed to create pixbuf from Cairo surface", .{});
            }
        } else {
            std.log.warn("No Cairo surface provided to status icon update", .{});
        }
        self.updateTooltip(tooltip);
    }
    
    pub fn updateTooltip(self: *Self, tooltip: []const u8) void {
        var tooltip_z: [256]u8 = undefined;
        const tooltip_str = std.fmt.bufPrintZ(&tooltip_z, "{s}", .{tooltip}) catch return;
        c.gtk_status_icon_set_tooltip_text(self.status_icon, tooltip_str);
    }
    
    pub fn setVisible(self: *Self, visible: bool) void {
        c.gtk_status_icon_set_visible(self.status_icon, if (visible) @as(c.gboolean, 1) else @as(c.gboolean, 0));
    }
    
    pub fn isEmbedded(self: *const Self) bool {
        return c.gtk_status_icon_is_embedded(self.status_icon) == @as(c.gboolean, 1);
    }
    
    fn onTrayActivate(status_icon: *c.GtkStatusIcon, tray: *Self) callconv(.C) void {
        _ = status_icon;
        
        if (tray.callback) |cb| {
            cb("activate", tray.user_data);
        }
    }
    
    fn onTrayPopupMenu(status_icon: *c.GtkStatusIcon, button: c.guint, activate_time: c.guint, tray: *Self) callconv(.C) void {
        _ = status_icon;
        _ = button;
        _ = activate_time;
        
        if (tray.callback) |cb| {
            cb("popup-menu", tray.user_data);
        }
    }
};