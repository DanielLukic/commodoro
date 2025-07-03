const std = @import("std");
const cw = @import("../c_workaround.zig");
const c = cw.c;

pub const BreakOverlay = struct {
    allocator: std.mem.Allocator,
    
    // GTK Widgets
    window: *c.GtkWidget,
    main_box: *c.GtkWidget,
    title_label: *c.GtkWidget,
    time_label: *c.GtkWidget,
    message_label: *c.GtkWidget,
    button_box: *c.GtkWidget,
    skip_button: *c.GtkWidget,
    extend_button: *c.GtkWidget,
    pause_button: *c.GtkWidget,
    dismiss_label: *c.GtkWidget,
    
    // Multi-monitor support
    secondary_windows: std.ArrayList(*c.GtkWidget),
    
    // Callback
    callback: ?*const fn (action: []const u8, user_data: ?*anyopaque) void,
    user_data: ?*anyopaque,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator) !*Self {
        const overlay = try allocator.create(Self);
        errdefer allocator.destroy(overlay);
        
        // Create fullscreen window
        const window = c.gtk_window_new(@intFromEnum(c.GtkWindowType.GTK_WINDOW_TOPLEVEL)) orelse return error.WindowCreationFailed;
        c.gtk_window_set_title(@ptrCast(window), "Break Time");
        c.gtk_window_set_decorated(@ptrCast(window), c.FALSE);
        c.gtk_window_set_skip_taskbar_hint(@ptrCast(window), c.TRUE);
        c.gtk_window_set_skip_pager_hint(@ptrCast(window), c.TRUE);
        c.gtk_window_set_keep_above(@ptrCast(window), c.TRUE);
        c.gtk_window_stick(@ptrCast(window)); // Show on all workspaces/desktops
        c.gtk_window_fullscreen(@ptrCast(window));
        
        // Set black background
        const context = c.gtk_widget_get_style_context(window);
        c.gtk_style_context_add_class(context, "break-overlay");
        
        // Create main container
        const main_box = c.gtk_box_new(@intFromEnum(c.GtkOrientation.GTK_ORIENTATION_VERTICAL), 40) orelse return error.WidgetCreationFailed;
        c.gtk_widget_set_halign(main_box, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_widget_set_valign(main_box, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_container_add(@ptrCast(window), main_box);
        
        // Break type title
        const title_label = c.gtk_label_new("Short Break") orelse return error.WidgetCreationFailed;
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(title_label), "break-title");
        c.gtk_widget_set_halign(title_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_box_pack_start(@ptrCast(main_box), title_label, c.FALSE, c.FALSE, 0);
        
        // Countdown timer
        const time_label = c.gtk_label_new("04:04") orelse return error.WidgetCreationFailed;
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(time_label), "break-timer");
        c.gtk_widget_set_halign(time_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_box_pack_start(@ptrCast(main_box), time_label, c.FALSE, c.FALSE, 0);
        
        // Motivational message
        const message_label = c.gtk_label_new("Take a quick breather") orelse return error.WidgetCreationFailed;
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(message_label), "break-message");
        c.gtk_widget_set_halign(message_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_box_pack_start(@ptrCast(main_box), message_label, c.FALSE, c.FALSE, 0);
        
        // Button container
        const button_box = c.gtk_box_new(@intFromEnum(c.GtkOrientation.GTK_ORIENTATION_HORIZONTAL), 20) orelse return error.WidgetCreationFailed;
        c.gtk_widget_set_halign(button_box, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_box_pack_start(@ptrCast(main_box), button_box, c.FALSE, c.FALSE, 0);
        
        // Skip Break button
        const skip_button = c.gtk_button_new_with_label("Skip Break (S)") orelse return error.WidgetCreationFailed;
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(skip_button), "break-button");
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(skip_button), "break-button-destructive");
        c.gtk_widget_set_size_request(skip_button, 140, 40);
        c.gtk_box_pack_start(@ptrCast(button_box), skip_button, c.FALSE, c.FALSE, 0);
        
        // Extend Break button
        const extend_button = c.gtk_button_new_with_label("Extend Break (E)") orelse return error.WidgetCreationFailed;
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(extend_button), "break-button");
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(extend_button), "break-button-normal");
        c.gtk_widget_set_size_request(extend_button, 140, 40);
        c.gtk_box_pack_start(@ptrCast(button_box), extend_button, c.FALSE, c.FALSE, 0);
        
        // Pause button
        const pause_button = c.gtk_button_new_with_label("Pause (P)") orelse return error.WidgetCreationFailed;
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(pause_button), "break-button");
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(pause_button), "break-button-warning");
        c.gtk_widget_set_size_request(pause_button, 140, 40);
        c.gtk_box_pack_start(@ptrCast(button_box), pause_button, c.FALSE, c.FALSE, 0);
        
        // ESC to dismiss label
        const dismiss_label = c.gtk_label_new("Press ESC to dismiss") orelse return error.WidgetCreationFailed;
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(dismiss_label), "break-dismiss");
        c.gtk_widget_set_halign(dismiss_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_widget_set_margin_top(dismiss_label, 40);
        c.gtk_box_pack_start(@ptrCast(main_box), dismiss_label, c.FALSE, c.FALSE, 0);
        
        overlay.* = .{
            .allocator = allocator,
            .window = window,
            .main_box = main_box,
            .title_label = title_label,
            .time_label = time_label,
            .message_label = message_label,
            .button_box = button_box,
            .skip_button = skip_button,
            .extend_button = extend_button,
            .pause_button = pause_button,
            .dismiss_label = dismiss_label,
            .secondary_windows = std.ArrayList(*c.GtkWidget).init(allocator),
            .callback = null,
            .user_data = null,
        };
        
        // Connect signals
        _ = c.g_signal_connect_data(skip_button, "clicked", @ptrCast(&onSkipClicked), overlay, null, 0);
        _ = c.g_signal_connect_data(extend_button, "clicked", @ptrCast(&onExtendClicked), overlay, null, 0);
        _ = c.g_signal_connect_data(pause_button, "clicked", @ptrCast(&onPauseClicked), overlay, null, 0);
        _ = c.g_signal_connect_data(window, "key-press-event", @ptrCast(&onKeyPress), overlay, null, 0);
        
        // Load CSS for styling
        overlay.applyStyling();
        
        // Initially hidden
        c.gtk_widget_hide(window);
        
        return overlay;
    }
    
    pub fn deinit(self: *Self) void {
        self.destroySecondaryOverlays();
        self.secondary_windows.deinit();
        c.gtk_widget_destroy(self.window);
        self.allocator.destroy(self);
    }
    
    pub fn setCallback(self: *Self, callback: *const fn ([]const u8, ?*anyopaque) void, user_data: ?*anyopaque) void {
        self.callback = callback;
        self.user_data = user_data;
    }
    
    pub fn show(self: *Self, break_type: []const u8, minutes: i32, seconds: i32) void {
        // Update content
        self.updateType(break_type);
        self.updateTime(minutes, seconds);
        
        // Create secondary overlays for other monitors
        self.createSecondaryOverlays() catch {};
        
        // Show main window
        c.gtk_widget_show_all(self.window);
        c.gtk_window_present(@ptrCast(self.window));
        
        // Show secondary windows
        for (self.secondary_windows.items) |secondary| {
            c.gtk_widget_show_all(secondary);
            c.gtk_window_present(@ptrCast(secondary));
        }
        
        // Grab focus to ensure key events are captured
        c.gtk_widget_grab_focus(self.window);
    }
    
    pub fn hide(self: *Self) void {
        c.gtk_widget_hide(self.window);
        
        // Hide and destroy secondary windows
        self.destroySecondaryOverlays();
    }
    
    pub fn updateTime(self: *Self, minutes: i32, seconds: i32) void {
        var time_text: [16]u8 = undefined;
        const time_str = std.fmt.bufPrintZ(&time_text, "{d:0>2}:{d:0>2}", .{ @as(u32, @intCast(minutes)), @as(u32, @intCast(seconds)) }) catch "00:00";
        c.gtk_label_set_text(@ptrCast(self.time_label), time_str);
    }
    
    pub fn updateType(self: *Self, break_type: []const u8) void {
        var type_buf: [32]u8 = undefined;
        const type_str = std.fmt.bufPrintZ(&type_buf, "{s}", .{break_type}) catch return;
        c.gtk_label_set_text(@ptrCast(self.title_label), type_str);
        
        const message = self.getMotivationalMessage(break_type);
        c.gtk_label_set_text(@ptrCast(self.message_label), message);
        
        // Update title color based on break type
        const context = c.gtk_widget_get_style_context(self.title_label);
        c.gtk_style_context_remove_class(context, "break-title-short");
        c.gtk_style_context_remove_class(context, "break-title-long");
        c.gtk_style_context_remove_class(context, "break-title-paused");
        
        if (std.mem.eql(u8, break_type, "Short Break")) {
            c.gtk_style_context_add_class(context, "break-title-short");
        } else if (std.mem.eql(u8, break_type, "Long Break")) {
            c.gtk_style_context_add_class(context, "break-title-long");
        } else if (std.mem.eql(u8, break_type, "Paused")) {
            c.gtk_style_context_add_class(context, "break-title-paused");
        }
    }
    
    pub fn isVisible(self: *const Self) bool {
        return c.gtk_widget_get_visible(self.window) == 1;
    }
    
    pub fn updatePauseButton(self: *Self, label: []const u8) void {
        var label_buf: [32]u8 = undefined;
        const label_str = if (std.mem.eql(u8, label, "Pause")) 
            std.fmt.bufPrintZ(&label_buf, "Pause (P)", .{}) catch "Pause (P)"
        else if (std.mem.eql(u8, label, "Resume")) 
            std.fmt.bufPrintZ(&label_buf, "Resume (R)", .{}) catch "Resume (R)"
        else 
            std.fmt.bufPrintZ(&label_buf, "{s}", .{label}) catch "Pause";
        
        c.gtk_button_set_label(@ptrCast(self.pause_button), label_str.ptr);
    }
    
    fn applyStyling(self: *Self) void {
        _ = self;
        const css_provider = c.gtk_css_provider_new();
        const css_data =
            \\.break-overlay { background-color: #000000; }
            \\.break-title { font-size: 48px; color: #4dd0e1; font-weight: bold; }
            \\.break-timer { font-size: 96px; color: #ffffff; font-weight: bold; font-family: monospace; }
            \\.break-message { font-size: 24px; color: #888888; }
            \\.break-button { 
            \\  min-width: 120px; min-height: 40px; 
            \\  font-size: 16px; font-weight: bold; 
            \\  border-radius: 8px; 
            \\}
            \\.break-button-destructive { 
            \\  background-color: #d32f2f; color: #ffffff; 
            \\  border: 2px solid #b71c1c; 
            \\}
            \\.break-button-destructive:hover { background-color: #f44336; }
            \\.break-button-normal { 
            \\  background-color: #1976d2; color: #ffffff; 
            \\  border: 2px solid #0d47a1; 
            \\}
            \\.break-button-normal:hover { background-color: #2196f3; }
            \\.break-button-warning { 
            \\  background-color: #f57c00; color: #ffffff; 
            \\  border: 2px solid #e65100; 
            \\}
            \\.break-button-warning:hover { background-color: #ff9800; }
            \\.break-dismiss { font-size: 16px; color: #666666; }
            \\.break-title-short { color: #4dd0e1; }
            \\.break-title-long { color: #66bb6a; }
            \\.break-title-paused { color: #ffa726; }
        ;
        
        _ = c.gtk_css_provider_load_from_data(css_provider, css_data, css_data.len, null);
        
        c.gtk_style_context_add_provider_for_screen(
            c.gdk_screen_get_default(),
            @ptrCast(css_provider),
            c.GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        
        c.g_object_unref(css_provider);
    }
    
    fn getMotivationalMessage(self: *const Self, break_type: []const u8) [*:0]const u8 {
        _ = self;
        if (std.mem.eql(u8, break_type, "Short Break")) {
            return "Take a quick breather";
        } else if (std.mem.eql(u8, break_type, "Long Break")) {
            return "Time for a longer rest";
        } else if (std.mem.eql(u8, break_type, "Paused")) {
            return "Break timer paused";
        } else {
            return "Take a moment to relax";
        }
    }
    
    fn createSecondaryOverlays(self: *Self) !void {
        // Clean up any existing secondary windows first
        self.destroySecondaryOverlays();
        
        // Get the default display and number of monitors
        const display = c.gdk_display_get_default();
        const n_monitors = c.gdk_display_get_n_monitors(display);
        
        // Get the primary monitor (where the main overlay will be shown)
        var primary_monitor = c.gdk_display_get_primary_monitor(display);
        if (primary_monitor == null and n_monitors > 0) {
            // Fallback to first monitor if no primary is set
            primary_monitor = c.gdk_display_get_monitor(display, 0);
        }
        
        // Create overlay windows for all other monitors
        var i: c_int = 0;
        while (i < n_monitors) : (i += 1) {
            const monitor = c.gdk_display_get_monitor(display, i);
            
            // Skip the monitor that has the main overlay
            if (monitor == primary_monitor) {
                continue;
            }
            
            // Get monitor geometry
            var geometry: c.GdkRectangle = undefined;
            c.gdk_monitor_get_geometry(monitor, &geometry);
            
            // Create a simple overlay window
            const secondary_window = c.gtk_window_new(@intFromEnum(c.GtkWindowType.GTK_WINDOW_TOPLEVEL)) orelse continue;
            c.gtk_window_set_decorated(@ptrCast(secondary_window), c.FALSE);
            c.gtk_window_set_skip_taskbar_hint(@ptrCast(secondary_window), c.TRUE);
            c.gtk_window_set_skip_pager_hint(@ptrCast(secondary_window), c.TRUE);
            c.gtk_window_set_keep_above(@ptrCast(secondary_window), c.TRUE);
            c.gtk_window_stick(@ptrCast(secondary_window));
            
            // Position and size the window to cover the monitor
            c.gtk_window_move(@ptrCast(secondary_window), geometry.x, geometry.y);
            c.gtk_window_set_default_size(@ptrCast(secondary_window), geometry.width, geometry.height);
            c.gtk_window_fullscreen(@ptrCast(secondary_window));
            
            // Set black background
            const context = c.gtk_widget_get_style_context(secondary_window);
            c.gtk_style_context_add_class(context, "break-overlay");
            
            // Connect key press event to allow ESC on any monitor
            _ = c.g_signal_connect_data(secondary_window, "key-press-event", @ptrCast(&onKeyPress), self, null, 0);
            
            // Add to list
            try self.secondary_windows.append(secondary_window);
        }
    }
    
    fn destroySecondaryOverlays(self: *Self) void {
        // Destroy all secondary windows
        for (self.secondary_windows.items) |window| {
            c.gtk_widget_destroy(window);
        }
        
        // Clear the list
        self.secondary_windows.clearRetainingCapacity();
    }
    
    // Event handlers
    fn onSkipClicked(button: *c.GtkButton, user_data: c.gpointer) callconv(.C) void {
        _ = button;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        if (self.callback) |cb| {
            cb("skip_break", self.user_data);
        }
    }
    
    fn onExtendClicked(button: *c.GtkButton, user_data: c.gpointer) callconv(.C) void {
        _ = button;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        if (self.callback) |cb| {
            cb("extend_break", self.user_data);
        }
    }
    
    fn onPauseClicked(button: *c.GtkButton, user_data: c.gpointer) callconv(.C) void {
        _ = button;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        if (self.callback) |cb| {
            cb("pause", self.user_data);
        }
    }
    
    fn onKeyPress(widget: *c.GtkWidget, event: *c.GdkEventKey, user_data: c.gpointer) callconv(.C) c.gboolean {
        _ = widget;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        if (event.keyval == c.GDK_KEY_Escape) {
            if (self.callback) |cb| {
                cb("dismiss", self.user_data);
            }
            return c.TRUE;
        }
        
        // Handle keyboard shortcuts for buttons
        switch (event.keyval) {
            c.GDK_KEY_s, c.GDK_KEY_S => {
                // Skip break
                if (self.callback) |cb| {
                    cb("skip_break", self.user_data);
                }
                return c.TRUE;
            },
            
            c.GDK_KEY_e, c.GDK_KEY_E => {
                // Extend break
                if (self.callback) |cb| {
                    cb("extend_break", self.user_data);
                }
                return c.TRUE;
            },
            
            c.GDK_KEY_p, c.GDK_KEY_P => {
                // Pause break
                if (self.callback) |cb| {
                    cb("pause", self.user_data);
                }
                return c.TRUE;
            },
            
            c.GDK_KEY_r, c.GDK_KEY_R => {
                // Resume break (same as pause - toggles)
                if (self.callback) |cb| {
                    cb("pause", self.user_data);
                }
                return c.TRUE;
            },
            
            else => {},
        }
        
        return c.FALSE;
    }
};