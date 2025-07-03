const std = @import("std");
const cw = @import("../c_workaround.zig");
const c = cw.c;
const Settings = @import("../config/settings.zig").Settings;

pub const SettingsDialog = struct {
    allocator: std.mem.Allocator,
    
    // GTK Widgets
    dialog: *c.GtkWidget,
    notebook: *c.GtkWidget,
    
    // Timer tab widgets
    work_duration_spin: *c.GtkWidget,
    short_break_spin: *c.GtkWidget,
    long_break_spin: *c.GtkWidget,
    sessions_spin: *c.GtkWidget,
    
    // Misc tab widgets
    auto_start_check: *c.GtkWidget,
    enable_sounds_check: *c.GtkWidget,
    enable_idle_detection_check: *c.GtkWidget,
    idle_timeout_spin: *c.GtkWidget,
    idle_timeout_box: *c.GtkWidget,
    
    // Dialog buttons
    restore_defaults_button: *c.GtkWidget,
    cancel_button: *c.GtkWidget,
    ok_button: *c.GtkWidget,
    
    // Callback
    callback: ?*const fn (action: []const u8, user_data: ?*anyopaque) void,
    user_data: ?*anyopaque,
    
    // Current settings
    settings: *Settings,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator, parent: ?*c.GtkWindow, settings: *Settings) !*Self {
        const dialog_self = try allocator.create(Self);
        errdefer allocator.destroy(dialog_self);
        
        // Create dialog
        const dialog = c.gtk_dialog_new() orelse return error.DialogCreationFailed;
        c.gtk_window_set_title(@ptrCast(dialog), "Settings");
        if (parent) |p| {
            c.gtk_window_set_transient_for(@ptrCast(dialog), p);
        }
        c.gtk_window_set_modal(@ptrCast(dialog), c.TRUE);
        c.gtk_window_set_default_size(@ptrCast(dialog), 400, 300);
        c.gtk_window_set_resizable(@ptrCast(dialog), c.FALSE);
        
        // Get content area
        const content_area = c.gtk_dialog_get_content_area(@ptrCast(dialog));
        
        // Create notebook for tabs
        const notebook = c.gtk_notebook_new() orelse return error.WidgetCreationFailed;
        c.gtk_container_add(@ptrCast(content_area), notebook);
        
        // Create Timer tab
        const timer_tab = c.gtk_box_new(@intFromEnum(c.GtkOrientation.GTK_ORIENTATION_VERTICAL), 10) orelse return error.WidgetCreationFailed;
        c.gtk_container_set_border_width(@ptrCast(timer_tab), 20);
        
        const timer_label = c.gtk_label_new("Timer") orelse return error.WidgetCreationFailed;
        _ = c.gtk_notebook_append_page(@ptrCast(notebook), timer_tab, timer_label);
        
        // Timer Durations section
        const durations_frame = c.gtk_frame_new("Timer Durations") orelse return error.WidgetCreationFailed;
        c.gtk_box_pack_start(@ptrCast(timer_tab), durations_frame, c.FALSE, c.FALSE, 0);
        
        const durations_grid = c.gtk_grid_new() orelse return error.WidgetCreationFailed;
        c.gtk_grid_set_row_spacing(@ptrCast(durations_grid), 10);
        c.gtk_grid_set_column_spacing(@ptrCast(durations_grid), 10);
        c.gtk_container_set_border_width(@ptrCast(durations_grid), 15);
        c.gtk_container_add(@ptrCast(durations_frame), durations_grid);
        
        // Work Duration
        const work_label = c.gtk_label_new("Work Duration:") orelse return error.WidgetCreationFailed;
        c.gtk_widget_set_halign(work_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_START));
        c.gtk_grid_attach(@ptrCast(durations_grid), work_label, 0, 0, 1, 1);
        
        const work_duration_spin = c.gtk_spin_button_new_with_range(1, 120, 1) orelse return error.WidgetCreationFailed;
        c.gtk_spin_button_set_value(@ptrCast(work_duration_spin), @floatFromInt(settings.work_duration));
        c.gtk_grid_attach(@ptrCast(durations_grid), work_duration_spin, 1, 0, 1, 1);
        
        const work_min_label = c.gtk_label_new("min") orelse return error.WidgetCreationFailed;
        c.gtk_grid_attach(@ptrCast(durations_grid), work_min_label, 2, 0, 1, 1);
        
        // Short Break
        const short_break_label = c.gtk_label_new("Short Break:") orelse return error.WidgetCreationFailed;
        c.gtk_widget_set_halign(short_break_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_START));
        c.gtk_grid_attach(@ptrCast(durations_grid), short_break_label, 0, 1, 1, 1);
        
        const short_break_spin = c.gtk_spin_button_new_with_range(1, 60, 1) orelse return error.WidgetCreationFailed;
        c.gtk_spin_button_set_value(@ptrCast(short_break_spin), @floatFromInt(settings.short_break_duration));
        c.gtk_grid_attach(@ptrCast(durations_grid), short_break_spin, 1, 1, 1, 1);
        
        const short_min_label = c.gtk_label_new("min") orelse return error.WidgetCreationFailed;
        c.gtk_grid_attach(@ptrCast(durations_grid), short_min_label, 2, 1, 1, 1);
        
        // Long Break
        const long_break_label = c.gtk_label_new("Long Break:") orelse return error.WidgetCreationFailed;
        c.gtk_widget_set_halign(long_break_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_START));
        c.gtk_grid_attach(@ptrCast(durations_grid), long_break_label, 0, 2, 1, 1);
        
        const long_break_spin = c.gtk_spin_button_new_with_range(5, 120, 1) orelse return error.WidgetCreationFailed;
        c.gtk_spin_button_set_value(@ptrCast(long_break_spin), @floatFromInt(settings.long_break_duration));
        c.gtk_grid_attach(@ptrCast(durations_grid), long_break_spin, 1, 2, 1, 1);
        
        const long_min_label = c.gtk_label_new("min") orelse return error.WidgetCreationFailed;
        c.gtk_grid_attach(@ptrCast(durations_grid), long_min_label, 2, 2, 1, 1);
        
        // Sessions until long break
        const sessions_label = c.gtk_label_new("Sessions until Long Break:") orelse return error.WidgetCreationFailed;
        c.gtk_widget_set_halign(sessions_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_START));
        c.gtk_grid_attach(@ptrCast(durations_grid), sessions_label, 0, 3, 1, 1);
        
        const sessions_spin = c.gtk_spin_button_new_with_range(2, 10, 1) orelse return error.WidgetCreationFailed;
        c.gtk_spin_button_set_value(@ptrCast(sessions_spin), @floatFromInt(settings.sessions_until_long_break));
        c.gtk_grid_attach(@ptrCast(durations_grid), sessions_spin, 1, 3, 1, 1);
        
        // Create Misc tab
        const misc_tab = c.gtk_box_new(@intFromEnum(c.GtkOrientation.GTK_ORIENTATION_VERTICAL), 10) orelse return error.WidgetCreationFailed;
        c.gtk_container_set_border_width(@ptrCast(misc_tab), 20);
        
        const misc_label = c.gtk_label_new("Misc") orelse return error.WidgetCreationFailed;
        _ = c.gtk_notebook_append_page(@ptrCast(notebook), misc_tab, misc_label);
        
        // Behavior Settings
        const behavior_frame = c.gtk_frame_new("Behavior Settings") orelse return error.WidgetCreationFailed;
        c.gtk_box_pack_start(@ptrCast(misc_tab), behavior_frame, c.FALSE, c.FALSE, 0);
        
        const behavior_box = c.gtk_box_new(@intFromEnum(c.GtkOrientation.GTK_ORIENTATION_VERTICAL), 10) orelse return error.WidgetCreationFailed;
        c.gtk_container_set_border_width(@ptrCast(behavior_box), 15);
        c.gtk_container_add(@ptrCast(behavior_frame), behavior_box);
        
        const auto_start_check = c.gtk_check_button_new_with_label("Auto-start work after breaks end") orelse return error.WidgetCreationFailed;
        c.gtk_toggle_button_set_active(@ptrCast(auto_start_check), if (settings.auto_start_work_after_break) c.TRUE else c.FALSE);
        c.gtk_widget_set_margin_bottom(auto_start_check, 8);
        c.gtk_box_pack_start(@ptrCast(behavior_box), auto_start_check, c.FALSE, c.FALSE, 0);
        
        const enable_sounds_check = c.gtk_check_button_new_with_label("Enable sound alerts") orelse return error.WidgetCreationFailed;
        c.gtk_toggle_button_set_active(@ptrCast(enable_sounds_check), if (settings.enable_sounds) c.TRUE else c.FALSE);
        c.gtk_box_pack_start(@ptrCast(behavior_box), enable_sounds_check, c.FALSE, c.FALSE, 0);
        
        // Idle detection section
        c.gtk_widget_set_margin_top(enable_sounds_check, 8);
        const enable_idle_detection_check = c.gtk_check_button_new_with_label("Auto-pause when idle") orelse return error.WidgetCreationFailed;
        c.gtk_toggle_button_set_active(@ptrCast(enable_idle_detection_check), if (settings.enable_idle_detection) c.TRUE else c.FALSE);
        c.gtk_widget_set_margin_top(enable_idle_detection_check, 8);
        c.gtk_box_pack_start(@ptrCast(behavior_box), enable_idle_detection_check, c.FALSE, c.FALSE, 0);
        
        // Idle timeout configuration
        const idle_timeout_box = c.gtk_box_new(@intFromEnum(c.GtkOrientation.GTK_ORIENTATION_HORIZONTAL), 10) orelse return error.WidgetCreationFailed;
        c.gtk_widget_set_margin_left(idle_timeout_box, 24);
        c.gtk_widget_set_margin_top(idle_timeout_box, 5);
        c.gtk_box_pack_start(@ptrCast(behavior_box), idle_timeout_box, c.FALSE, c.FALSE, 0);
        
        const idle_timeout_label = c.gtk_label_new("Idle timeout:") orelse return error.WidgetCreationFailed;
        c.gtk_box_pack_start(@ptrCast(idle_timeout_box), idle_timeout_label, c.FALSE, c.FALSE, 0);
        
        const idle_timeout_spin = c.gtk_spin_button_new_with_range(1, 30, 1) orelse return error.WidgetCreationFailed;
        c.gtk_spin_button_set_value(@ptrCast(idle_timeout_spin), @floatFromInt(settings.idle_timeout_minutes));
        c.gtk_box_pack_start(@ptrCast(idle_timeout_box), idle_timeout_spin, c.FALSE, c.FALSE, 0);
        
        const idle_min_label = c.gtk_label_new("minutes") orelse return error.WidgetCreationFailed;
        c.gtk_box_pack_start(@ptrCast(idle_timeout_box), idle_min_label, c.FALSE, c.FALSE, 0);
        
        // Set sensitivity based on checkbox state
        c.gtk_widget_set_sensitive(idle_timeout_box, if (settings.enable_idle_detection) c.TRUE else c.FALSE);
        
        // Dialog buttons
        const action_area = c.gtk_dialog_get_action_area(@ptrCast(dialog));
        
        const restore_defaults_button = c.gtk_button_new_with_label("Restore Defaults") orelse return error.WidgetCreationFailed;
        c.gtk_container_add(@ptrCast(action_area), restore_defaults_button);
        
        const cancel_button = c.gtk_button_new_with_label("Cancel") orelse return error.WidgetCreationFailed;
        c.gtk_container_add(@ptrCast(action_area), cancel_button);
        
        const ok_button = c.gtk_button_new_with_label("OK") orelse return error.WidgetCreationFailed;
        c.gtk_style_context_add_class(c.gtk_widget_get_style_context(ok_button), "suggested-action");
        c.gtk_container_add(@ptrCast(action_area), ok_button);
        
        dialog_self.* = .{
            .allocator = allocator,
            .dialog = dialog,
            .notebook = notebook,
            .work_duration_spin = work_duration_spin,
            .short_break_spin = short_break_spin,
            .long_break_spin = long_break_spin,
            .sessions_spin = sessions_spin,
            .auto_start_check = auto_start_check,
            .enable_sounds_check = enable_sounds_check,
            .enable_idle_detection_check = enable_idle_detection_check,
            .idle_timeout_spin = idle_timeout_spin,
            .idle_timeout_box = idle_timeout_box,
            .restore_defaults_button = restore_defaults_button,
            .cancel_button = cancel_button,
            .ok_button = ok_button,
            .callback = null,
            .user_data = null,
            .settings = settings,
        };
        
        // Connect signals
        _ = c.g_signal_connect_data(restore_defaults_button, "clicked", @ptrCast(&onRestoreDefaultsClicked), dialog_self, null, 0);
        _ = c.g_signal_connect_data(cancel_button, "clicked", @ptrCast(&onCancelClicked), dialog_self, null, 0);
        _ = c.g_signal_connect_data(ok_button, "clicked", @ptrCast(&onOkClicked), dialog_self, null, 0);
        _ = c.g_signal_connect_data(enable_idle_detection_check, "toggled", @ptrCast(&onIdleDetectionToggled), dialog_self, null, 0);
        
        return dialog_self;
    }
    
    pub fn deinit(self: *Self) void {
        c.gtk_widget_destroy(self.dialog);
        self.allocator.destroy(self);
    }
    
    pub fn setCallback(self: *Self, callback: *const fn ([]const u8, ?*anyopaque) void, user_data: ?*anyopaque) void {
        self.callback = callback;
        self.user_data = user_data;
    }
    
    pub fn show(self: *Self) void {
        c.gtk_widget_show_all(self.dialog);
    }
    
    pub fn getSettings(self: *Self) Settings {
        var new_settings = Settings.init(self.allocator);
        
        // Get values from spinners
        new_settings.work_duration = @intFromFloat(c.gtk_spin_button_get_value(@ptrCast(self.work_duration_spin)));
        new_settings.short_break_duration = @intFromFloat(c.gtk_spin_button_get_value(@ptrCast(self.short_break_spin)));
        new_settings.long_break_duration = @intFromFloat(c.gtk_spin_button_get_value(@ptrCast(self.long_break_spin)));
        new_settings.sessions_until_long_break = @intFromFloat(c.gtk_spin_button_get_value(@ptrCast(self.sessions_spin)));
        
        // Get checkbox states
        new_settings.auto_start_work_after_break = c.gtk_toggle_button_get_active(@ptrCast(self.auto_start_check)) == c.TRUE;
        new_settings.enable_sounds = c.gtk_toggle_button_get_active(@ptrCast(self.enable_sounds_check)) == c.TRUE;
        new_settings.enable_idle_detection = c.gtk_toggle_button_get_active(@ptrCast(self.enable_idle_detection_check)) == c.TRUE;
        new_settings.idle_timeout_minutes = @intFromFloat(c.gtk_spin_button_get_value(@ptrCast(self.idle_timeout_spin)));
        
        return new_settings;
    }
    
    fn updateFromSettings(self: *Self, settings: *const Settings) void {
        // Update spinners
        c.gtk_spin_button_set_value(@ptrCast(self.work_duration_spin), @floatFromInt(settings.work_duration));
        c.gtk_spin_button_set_value(@ptrCast(self.short_break_spin), @floatFromInt(settings.short_break_duration));
        c.gtk_spin_button_set_value(@ptrCast(self.long_break_spin), @floatFromInt(settings.long_break_duration));
        c.gtk_spin_button_set_value(@ptrCast(self.sessions_spin), @floatFromInt(settings.sessions_until_long_break));
        
        // Update checkboxes
        c.gtk_toggle_button_set_active(@ptrCast(self.auto_start_check), if (settings.auto_start_work_after_break) c.TRUE else c.FALSE);
        c.gtk_toggle_button_set_active(@ptrCast(self.enable_sounds_check), if (settings.enable_sounds) c.TRUE else c.FALSE);
        c.gtk_toggle_button_set_active(@ptrCast(self.enable_idle_detection_check), if (settings.enable_idle_detection) c.TRUE else c.FALSE);
        c.gtk_spin_button_set_value(@ptrCast(self.idle_timeout_spin), @floatFromInt(settings.idle_timeout_minutes));
    }
    
    // Event handlers
    fn onRestoreDefaultsClicked(button: *c.GtkButton, user_data: c.gpointer) callconv(.C) void {
        _ = button;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        // Create default settings and update UI
        var defaults = Settings.init(self.allocator);
        self.updateFromSettings(&defaults);
        
        if (self.callback) |cb| {
            cb("restore_defaults", self.user_data);
        }
    }
    
    fn onCancelClicked(button: *c.GtkButton, user_data: c.gpointer) callconv(.C) void {
        _ = button;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        c.gtk_widget_hide(self.dialog);
        
        if (self.callback) |cb| {
            cb("cancel", self.user_data);
        }
    }
    
    fn onOkClicked(button: *c.GtkButton, user_data: c.gpointer) callconv(.C) void {
        _ = button;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        c.gtk_widget_hide(self.dialog);
        
        if (self.callback) |cb| {
            cb("ok", self.user_data);
        }
    }
    
    fn onIdleDetectionToggled(button: *c.GtkToggleButton, user_data: c.gpointer) callconv(.C) void {
        const self: *Self = @ptrCast(@alignCast(user_data));
        const active = c.gtk_toggle_button_get_active(button) == c.TRUE;
        c.gtk_widget_set_sensitive(self.idle_timeout_box, if (active) c.TRUE else c.FALSE);
    }
};