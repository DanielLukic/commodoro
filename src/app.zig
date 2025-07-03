const std = @import("std");
// Use workaround to avoid GdkEvent opaque type issues
const cw = @import("c_workaround.zig");
const c = cw.c;
const Timer = @import("timer/timer.zig").Timer;
const TimerState = @import("timer/timer.zig").TimerState;
const AudioManager = @import("audio/audio.zig").AudioManager;
const TrayIcon = @import("gui/tray_icon.zig").TrayIcon;
const TrayStatusIcon = @import("gui/tray_status_icon.zig").TrayStatusIcon;
const BreakOverlay = @import("gui/break_overlay.zig").BreakOverlay;
const SettingsDialog = @import("gui/settings_dialog.zig").SettingsDialog;
const Settings = @import("config/settings.zig").Settings;
const ConfigManager = @import("config/config_manager.zig").ConfigManager;
const InputMonitor = @import("input/input_monitor.zig").InputMonitor;
const DBusService = @import("dbus/dbus_service.zig").DBusService;

pub const ZigodoroApp = struct {
    allocator: std.mem.Allocator,
    
    // GTK Widgets
    window: *c.GtkWidget,
    time_label: *c.GtkWidget,
    status_label: *c.GtkWidget,
    session_label: *c.GtkWidget,
    start_button: *c.GtkWidget,
    reset_button: *c.GtkWidget,
    settings_button: *c.GtkWidget,
    
    // Core components
    timer: *Timer,
    audio: *AudioManager,
    tray_icon: *TrayIcon,
    status_tray: *TrayStatusIcon,
    break_overlay: *BreakOverlay,
    settings: *Settings,
    settings_dialog: ?*SettingsDialog,
    config_manager: *ConfigManager,
    input_monitor: *InputMonitor,
    dbus_service: *DBusService,
    
    // State tracking
    paused_by_idle: bool,
    idle_check_source: c.guint,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator, cmd_args: anytype) !*Self {
        const app = try allocator.create(Self);
        errdefer allocator.destroy(app);
        
        // Create config manager
        const config_manager = try ConfigManager.init(allocator);
        errdefer config_manager.deinit();
        
        // Load or create settings
        const settings = try allocator.create(Settings);
        errdefer allocator.destroy(settings);
        
        if (!cmd_args.test_mode) {
            // Load settings from config file
            settings.* = config_manager.loadSettings() catch Settings.init(allocator);
        } else {
            // Use default settings for test mode
            settings.* = Settings.init(allocator);
        }
        
        // Create timer
        const timer = try Timer.init(allocator);
        errdefer timer.deinit();
        
        // Create audio manager
        const audio = try AudioManager.init(allocator);
        errdefer audio.deinit();
        
        // Create tray icon
        const tray_icon = try TrayIcon.init(allocator);
        errdefer tray_icon.deinit();
        try tray_icon.setTooltip("Zigodoro - Ready to start");
        
        // Create status tray
        const status_tray = try TrayStatusIcon.init(allocator);
        errdefer status_tray.deinit();
        
        // Create break overlay
        const break_overlay = try BreakOverlay.init(allocator);
        errdefer break_overlay.deinit();
        
        // Create input monitor
        const input_monitor = try InputMonitor.init(allocator);
        errdefer input_monitor.deinit();
        
        // Create D-Bus service (needs to be created after app struct is initialized)
        // We'll create it later after the app struct is fully initialized
        
        // Apply command line arguments if in test mode
        if (cmd_args.test_mode) {
            timer.setDurations(cmd_args.work_duration, cmd_args.short_break_duration, 
                             cmd_args.long_break_duration, cmd_args.sessions_until_long_break);
            timer.setDurationMode(true); // Use seconds mode for test
            
            // Also update settings
            settings.work_duration = cmd_args.work_duration;
            settings.short_break_duration = cmd_args.short_break_duration;
            settings.long_break_duration = cmd_args.long_break_duration;
            settings.sessions_until_long_break = cmd_args.sessions_until_long_break;
        } else {
            // Apply settings to timer
            timer.setDurations(settings.work_duration, settings.short_break_duration,
                             settings.long_break_duration, settings.sessions_until_long_break);
            timer.setAutoStartWork(settings.auto_start_work_after_break);
        }
        
        // Create main window
        const window = c.gtk_window_new(@intFromEnum(c.GtkWindowType.GTK_WINDOW_TOPLEVEL)) orelse return error.WindowCreationFailed;
        c.gtk_window_set_title(@ptrCast(window), "Zigodoro");
        c.gtk_window_set_default_size(@ptrCast(window), 400, 500);
        c.gtk_window_set_resizable(@ptrCast(window), c.FALSE);
        c.gtk_window_set_position(@ptrCast(window), @intFromEnum(c.GtkWindowPosition.GTK_WIN_POS_CENTER));
        
        // Create main container
        const main_box = c.gtk_box_new(@intFromEnum(c.GtkOrientation.GTK_ORIENTATION_VERTICAL), 20) orelse return error.WidgetCreationFailed;
        c.gtk_widget_set_margin_top(main_box, 30);
        c.gtk_widget_set_margin_bottom(main_box, 30);
        c.gtk_widget_set_margin_start(main_box, 30);
        c.gtk_widget_set_margin_end(main_box, 30);
        c.gtk_container_add(@ptrCast(window), main_box);
        
        // Time display
        const time_label = c.gtk_label_new("25:00") orelse return error.WidgetCreationFailed;
        const time_context = c.gtk_widget_get_style_context(time_label);
        c.gtk_style_context_add_class(time_context, "time-display");
        c.gtk_widget_set_halign(time_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_box_pack_start(@ptrCast(main_box), time_label, c.FALSE, c.FALSE, 0);
        
        // Status label
        const status_label = c.gtk_label_new("Ready to start") orelse return error.WidgetCreationFailed;
        const status_context = c.gtk_widget_get_style_context(status_label);
        c.gtk_style_context_add_class(status_context, "status-label");
        c.gtk_widget_set_halign(status_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_box_pack_start(@ptrCast(main_box), status_label, c.FALSE, c.FALSE, 0);
        
        // Button container
        const button_box = c.gtk_box_new(@intFromEnum(c.GtkOrientation.GTK_ORIENTATION_HORIZONTAL), 10) orelse return error.WidgetCreationFailed;
        c.gtk_widget_set_halign(button_box, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_box_pack_start(@ptrCast(main_box), button_box, c.FALSE, c.FALSE, 0);
        
        // Control buttons
        const start_button = c.gtk_button_new_with_label("Start") orelse return error.WidgetCreationFailed;
        const start_context = c.gtk_widget_get_style_context(start_button);
        c.gtk_style_context_add_class(start_context, "control-button");
        c.gtk_box_pack_start(@ptrCast(button_box), start_button, c.FALSE, c.FALSE, 0);
        
        const reset_button = c.gtk_button_new_with_label("Reset") orelse return error.WidgetCreationFailed;
        const reset_context = c.gtk_widget_get_style_context(reset_button);
        c.gtk_style_context_add_class(reset_context, "control-button");
        c.gtk_box_pack_start(@ptrCast(button_box), reset_button, c.FALSE, c.FALSE, 0);
        
        const settings_button = c.gtk_button_new_with_label("Settings") orelse return error.WidgetCreationFailed;
        const settings_context = c.gtk_widget_get_style_context(settings_button);
        c.gtk_style_context_add_class(settings_context, "control-button");
        c.gtk_box_pack_start(@ptrCast(button_box), settings_button, c.FALSE, c.FALSE, 0);
        
        // Session counter
        const session_label = c.gtk_label_new("Session: 1") orelse return error.WidgetCreationFailed;
        const session_context = c.gtk_widget_get_style_context(session_label);
        c.gtk_style_context_add_class(session_context, "session-label");
        c.gtk_widget_set_halign(session_label, @intFromEnum(c.GtkAlign.GTK_ALIGN_CENTER));
        c.gtk_widget_set_margin_top(session_label, 20);
        c.gtk_box_pack_start(@ptrCast(main_box), session_label, c.FALSE, c.FALSE, 0);
        
        app.* = .{
            .allocator = allocator,
            .window = window,
            .time_label = time_label,
            .status_label = status_label,
            .session_label = session_label,
            .start_button = start_button,
            .reset_button = reset_button,
            .settings_button = settings_button,
            .timer = timer,
            .audio = audio,
            .tray_icon = tray_icon,
            .status_tray = status_tray,
            .break_overlay = break_overlay,
            .settings = settings,
            .settings_dialog = null,
            .config_manager = config_manager,
            .input_monitor = input_monitor,
            .dbus_service = undefined, // Will be initialized after app is created
            .paused_by_idle = false,
            .idle_check_source = 0,
        };
        
        // Create D-Bus service now that app is initialized
        const dbus_service = try DBusService.init(allocator, app);
        app.dbus_service = dbus_service;
        dbus_service.publish();
        
        // Set up timer callbacks
        timer.setCallbacks(onTimerStateChanged, onTimerTick, onTimerSessionComplete, app);
        
        // Set up tray callback
        status_tray.setCallback(onTrayStatusAction, app);
        
        // Set up break overlay callback
        break_overlay.setCallback(onBreakOverlayAction, app);
        
        // Set up input monitor
        input_monitor.setWindow(@ptrCast(window));
        input_monitor.setCallback(onInputActivityDetected, app);
        
        // Connect signals
        _ = c.g_signal_connect_data(start_button, "clicked", @ptrCast(&onStartClicked), app, null, 0);
        _ = c.g_signal_connect_data(reset_button, "clicked", @ptrCast(&onResetClicked), app, null, 0);
        _ = c.g_signal_connect_data(settings_button, "clicked", @ptrCast(&onSettingsClicked), app, null, 0);
        _ = c.g_signal_connect_data(window, "destroy", @ptrCast(&onWindowDestroy), app, null, 0);
        // Note: delete-event handling is not currently supported due to GdkEvent opaque type limitations in Zig
        // The window will destroy on close rather than hide
        
        // Add keyboard shortcuts using accelerators
        const accel_group = c.gtk_accel_group_new();
        c.gtk_window_add_accel_group(@ptrCast(window), accel_group);
        
        // Ctrl+Q to quit
        c.gtk_accel_group_connect(accel_group, 'q', c.GDK_CONTROL_MASK, c.GTK_ACCEL_VISIBLE,
            c.g_cclosure_new(@ptrCast(&onQuitAccel), app, null));
        
        // Escape to hide window
        c.gtk_accel_group_connect(accel_group, c.GDK_KEY_Escape, 0, c.GTK_ACCEL_VISIBLE,
            c.g_cclosure_new(@ptrCast(&onEscapeAccel), app, null));
        
        // Apply CSS styling
        app.applyStyling();
        
        // Initial display update
        app.updateDisplay();
        
        return app;
    }
    
    pub fn deinit(self: *Self) void {
        // Stop idle monitoring
        self.stopIdleMonitoring();
        
        self.timer.deinit();
        self.audio.deinit();
        self.tray_icon.deinit();
        self.status_tray.deinit();
        self.break_overlay.deinit();
        self.input_monitor.deinit();
        self.dbus_service.deinit();
        if (self.settings_dialog) |dialog| {
            dialog.deinit();
        }
        self.settings.deinit();
        self.allocator.destroy(self.settings);
        self.config_manager.deinit();
        self.allocator.destroy(self);
    }
    
    pub fn run(self: *Self) void {
        c.gtk_widget_show_all(self.window);
        c.gtk_main();
    }
    
    fn applyStyling(self: *Self) void {
        _ = self;
        const css_provider = c.gtk_css_provider_new();
        const css_data =
            \\window { background-color: #2b2b2b; color: #ffffff; }
            \\.time-display { font-size: 72px; font-weight: bold; color: #f4e4c1; }
            \\.status-label { font-size: 18px; color: #888888; margin-bottom: 20px; }
            \\.control-button { min-width: 80px; min-height: 40px; margin: 0 5px;
            \\  background-color: #404040; color: #ffffff; border: 1px solid #555555; }
            \\.control-button:hover { background-color: #505050; }
            \\.session-label { font-size: 16px; color: #ffffff; }
        ;
        
        _ = c.gtk_css_provider_load_from_data(css_provider, css_data, css_data.len, null);
        
        c.gtk_style_context_add_provider_for_screen(
            c.gdk_screen_get_default(),
            @ptrCast(css_provider),
            c.GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        
        c.g_object_unref(css_provider);
    }
    
    fn updateDisplay(self: *Self) void {
        var time_text: [16]u8 = undefined;
        var session_text: [32]u8 = undefined;
        
        // Get current timer state
        const state = self.timer.getState();
        const time = self.timer.getRemaining();
        const session = self.timer.getSession();
        
        // Update time display
        const time_str = std.fmt.bufPrintZ(&time_text, "{d:0>2}:{d:0>2}", .{ @as(u32, @intCast(time.minutes)), @as(u32, @intCast(time.seconds)) }) catch "00:00";
        c.gtk_label_set_text(@ptrCast(self.time_label), time_str);
        
        // Update session display
        const session_str = std.fmt.bufPrintZ(&session_text, "Session: {d}", .{session}) catch "Session: 1";
        c.gtk_label_set_text(@ptrCast(self.session_label), session_str);
        
        // Update status display
        const status = switch (state) {
            .idle => "Ready to start",
            .work => "Work Session",
            .short_break => "Short Break",
            .long_break => "Long Break",
            .paused => "Paused",
        };
        c.gtk_label_set_text(@ptrCast(self.status_label), status);
        
        // Update button labels based on state
        const button_label = switch (state) {
            .idle => "Start",
            .paused => "Resume",
            .work, .short_break, .long_break => "Pause",
        };
        c.gtk_button_set_label(@ptrCast(self.start_button), button_label);
        
        // Update tray icon
        self.tray_icon.update(state, self.timer.getRemaining().minutes * 60 + self.timer.getRemaining().seconds, self.timer.getTotalDuration()) catch {};
        
        // Update tray tooltip
        var tooltip_buf: [64]u8 = undefined;
        const tooltip = std.fmt.bufPrint(&tooltip_buf, "Zigodoro - {s} ({d:0>2}:{d:0>2} remaining)", .{ status, @as(u32, @intCast(time.minutes)), @as(u32, @intCast(time.seconds)) }) catch "Zigodoro";
        self.tray_icon.setTooltip(tooltip) catch {};
        
        // Get the Cairo surface from tray_icon and update status_tray with it
        const surface = self.tray_icon.getSurface();
        self.status_tray.update(surface, tooltip);
    }
    
    // Timer callbacks
    fn onTimerStateChanged(timer: *Timer, state: TimerState, user_data: ?*anyopaque) void {
        _ = timer;
        const self: *Self = @ptrCast(@alignCast(user_data.?));
        
        switch (state) {
            .work => {
                self.audio.playWorkStart();
                self.break_overlay.hide();
                // Start idle monitoring during work sessions
                if (self.settings.enable_idle_detection) {
                    self.startIdleMonitoring();
                }
            },
            .short_break => {
                self.audio.playBreakStart();
                const time = self.timer.getRemaining();
                self.break_overlay.show("Short Break", time.minutes, time.seconds);
                // Stop idle monitoring during breaks
                self.stopIdleMonitoring();
            },
            .long_break => {
                self.audio.playLongBreakStart();
                const time = self.timer.getRemaining();
                self.break_overlay.show("Long Break", time.minutes, time.seconds);
                // Stop idle monitoring during breaks
                self.stopIdleMonitoring();
            },
            .paused => {
                if (self.break_overlay.isVisible()) {
                    self.break_overlay.updateType("Paused");
                    self.break_overlay.updatePauseButton("Resume");
                }
            },
            .idle => {
                self.break_overlay.hide();
                // Stop idle monitoring when idle
                self.stopIdleMonitoring();
                // Start input monitoring for auto-start
                if (self.settings.auto_start_work_after_break) {
                    self.input_monitor.start();
                }
            },
        }
        
        self.updateDisplay();
    }
    
    fn onTimerTick(timer: *Timer, minutes: i32, seconds: i32, user_data: ?*anyopaque) void {
        _ = timer;
        const self: *Self = @ptrCast(@alignCast(user_data.?));
        
        // Update break overlay if visible
        if (self.break_overlay.isVisible()) {
            self.break_overlay.updateTime(minutes, seconds);
        }
        
        self.updateDisplay();
    }
    
    fn onTimerSessionComplete(timer: *Timer, completed_state: TimerState, user_data: ?*anyopaque) void {
        _ = timer;
        const self: *Self = @ptrCast(@alignCast(user_data.?));
        
        // Play appropriate sound based on what just completed
        switch (completed_state) {
            .work => self.audio.playSessionComplete(),
            .short_break, .long_break => self.audio.playTimerFinish(),
            else => {},
        }
    }
    
    // Button callbacks
    fn onStartClicked(button: *c.GtkButton, user_data: c.gpointer) callconv(.C) void {
        _ = button;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        const state = self.timer.getState();
        
        // Single button logic: Start/Pause/Resume
        if (state == .idle or state == .paused) {
            self.timer.start();
        } else if (state == .work or state == .short_break or state == .long_break) {
            self.timer.pause();
        }
    }
    
    fn onResetClicked(button: *c.GtkButton, user_data: c.gpointer) callconv(.C) void {
        _ = button;
        const self: *Self = @ptrCast(@alignCast(user_data));
        self.timer.reset();
    }
    
    fn onSettingsClicked(button: *c.GtkButton, user_data: c.gpointer) callconv(.C) void {
        _ = button;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        // Create settings dialog if it doesn't exist
        if (self.settings_dialog == null) {
            self.settings_dialog = SettingsDialog.init(self.allocator, @ptrCast(self.window), self.settings) catch {
                std.log.err("Failed to create settings dialog", .{});
                return;
            };
            if (self.settings_dialog) |dialog| {
                dialog.setCallback(onSettingsDialogAction, self);
            }
        }
        
        // Show the dialog
        if (self.settings_dialog) |dialog| {
            dialog.show();
        }
    }
    
    fn onWindowDestroy(widget: *c.GtkWidget, user_data: c.gpointer) callconv(.C) void {
        _ = widget;
        _ = user_data;
        c.gtk_main_quit();
    }
    
    fn onTrayStatusAction(action: []const u8, user_data: ?*anyopaque) void {
        const self: *Self = @ptrCast(@alignCast(user_data.?));
        
        if (std.mem.eql(u8, action, "activate")) {
            // Toggle main window visibility on click
            if (c.gtk_widget_get_visible(self.window) == 1) {
                c.gtk_widget_hide(self.window);
            } else {
                c.gtk_widget_show(self.window);
                c.gtk_window_present(@ptrCast(self.window));
                c.gtk_window_set_urgency_hint(@ptrCast(self.window), 1);
            }
        } else if (std.mem.eql(u8, action, "popup-menu")) {
            // Show context menu
            self.showTrayMenu();
        }
    }
    
    fn showTrayMenu(self: *Self) void {
        const menu = c.gtk_menu_new();
        
        // Timer control items
        const state = self.timer.getState();
        
        // Start/Pause/Resume item
        const control_label = switch (state) {
            .idle => "Start",
            .paused => "Resume",
            else => "Pause",
        };
        
        const control_item = c.gtk_menu_item_new_with_label(control_label);
        _ = c.g_signal_connect_data(control_item, "activate", @ptrCast(&onTrayStartClicked), self, null, 0);
        c.gtk_menu_shell_append(@ptrCast(menu), @ptrCast(control_item));
        
        // Reset item
        const reset_item = c.gtk_menu_item_new_with_label("Reset");
        c.gtk_widget_set_sensitive(@ptrCast(reset_item), if (state != .idle) 1 else 0);
        _ = c.g_signal_connect_data(reset_item, "activate", @ptrCast(&onTrayResetClicked), self, null, 0);
        c.gtk_menu_shell_append(@ptrCast(menu), @ptrCast(reset_item));
        
        // Separator
        const separator = c.gtk_separator_menu_item_new();
        c.gtk_menu_shell_append(@ptrCast(menu), @ptrCast(separator));
        
        // Quit menu item
        const quit_item = c.gtk_menu_item_new_with_label("Quit");
        _ = c.g_signal_connect_data(quit_item, "activate", @ptrCast(&onTrayQuitClicked), self, null, 0);
        c.gtk_menu_shell_append(@ptrCast(menu), @ptrCast(quit_item));
        
        // Show menu
        c.gtk_widget_show_all(@ptrCast(menu));
        c.gtk_menu_popup_at_pointer(@ptrCast(menu), null);
    }
    
    fn onTrayStartClicked(item: *c.GtkMenuItem, user_data: c.gpointer) callconv(.C) void {
        _ = item;
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        const state = self.timer.getState();
        if (state == .idle or state == .paused) {
            self.timer.start();
        } else {
            self.timer.pause();
        }
    }
    
    fn onTrayResetClicked(item: *c.GtkMenuItem, user_data: c.gpointer) callconv(.C) void {
        _ = item;
        const self: *Self = @ptrCast(@alignCast(user_data));
        self.timer.reset();
    }
    
    fn onTrayQuitClicked(item: *c.GtkMenuItem, user_data: c.gpointer) callconv(.C) void {
        _ = item;
        _ = user_data;
        c.gtk_main_quit();
    }
    
    fn onQuitAccel(accel_group: *c.GtkAccelGroup, acceleratable: *c.GObject, keyval: c.guint, modifier: c.guint, user_data: c.gpointer) callconv(.C) c.gboolean {
        _ = accel_group;
        _ = acceleratable;
        _ = keyval;
        _ = modifier;
        _ = user_data;
        c.gtk_main_quit();
        return 1; // TRUE - handled
    }
    
    fn onEscapeAccel(accel_group: *c.GtkAccelGroup, acceleratable: *c.GObject, keyval: c.guint, modifier: c.guint, user_data: c.gpointer) callconv(.C) c.gboolean {
        _ = accel_group;
        _ = acceleratable;
        _ = keyval;
        _ = modifier;
        const self: *Self = @ptrCast(@alignCast(user_data));
        c.gtk_widget_hide(self.window);
        return 1; // TRUE - handled
    }
    
    fn onBreakOverlayAction(action: []const u8, user_data: ?*anyopaque) void {
        const self: *Self = @ptrCast(@alignCast(user_data.?));
        
        if (std.mem.eql(u8, action, "skip_break")) {
            // Skip the current break and start work
            self.timer.reset();
            self.timer.start();
        } else if (std.mem.eql(u8, action, "extend_break")) {
            // Add 2 minutes to the break
            self.timer.extendBreak(120);
        } else if (std.mem.eql(u8, action, "pause")) {
            // Toggle pause/resume
            const state = self.timer.getState();
            if (state == .paused) {
                self.timer.start();
                self.break_overlay.updatePauseButton("Pause");
            } else {
                self.timer.pause();
            }
        } else if (std.mem.eql(u8, action, "dismiss")) {
            // Hide the overlay but keep timer running
            self.break_overlay.hide();
        }
    }
    
    fn onSettingsDialogAction(action: []const u8, user_data: ?*anyopaque) void {
        const self: *Self = @ptrCast(@alignCast(user_data.?));
        
        if (std.mem.eql(u8, action, "ok")) {
            // Apply the new settings
            if (self.settings_dialog) |dialog| {
                // Get new settings from dialog
                const new_settings = dialog.getSettings();
                
                // Apply timer settings
                self.timer.setDurations(
                    new_settings.work_duration,
                    new_settings.short_break_duration,
                    new_settings.long_break_duration,
                    new_settings.sessions_until_long_break
                );
                self.timer.setAutoStartWork(new_settings.auto_start_work_after_break);
                
                // Apply audio settings
                if (new_settings.enable_sounds != self.settings.enable_sounds) {
                    if (new_settings.enable_sounds) {
                        self.audio.enable();
                    } else {
                        self.audio.disable();
                    }
                }
                
                // Update stored settings
                self.settings.deinit();
                self.settings.* = new_settings;
                
                // Save settings to config file
                self.config_manager.saveSettings(self.settings) catch |err| {
                    std.log.err("Failed to save settings: {}", .{err});
                };
            }
        }
    }
    
    // Input monitoring functions
    fn startIdleMonitoring(self: *Self) void {
        if (!self.settings.enable_idle_detection) return;
        
        // Stop any existing idle check
        self.stopIdleMonitoring();
        
        // Start periodic idle checking (every 30 seconds)
        self.idle_check_source = c.g_timeout_add_seconds(30, checkIdleTimeout, self);
        std.log.info("Started idle monitoring (checking every 30 seconds)", .{});
    }
    
    fn stopIdleMonitoring(self: *Self) void {
        if (self.idle_check_source > 0) {
            _ = c.g_source_remove(self.idle_check_source);
            self.idle_check_source = 0;
            std.log.info("Stopped idle monitoring", .{});
        }
    }
    
    fn checkIdleTimeout(user_data: c.gpointer) callconv(.C) c.gboolean {
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        // Only check idle time during work sessions
        const state = self.timer.getState();
        if (state != .work) {
            return 1; // G_SOURCE_CONTINUE
        }
        
        // Get current idle time
        const idle_seconds = self.input_monitor.getIdleTime();
        if (idle_seconds < 0) {
            // Error getting idle time, continue checking
            return 1; // G_SOURCE_CONTINUE
        }
        
        const idle_timeout_seconds = self.settings.idle_timeout_minutes * 60;
        
        std.log.info("Idle check: {} seconds idle (timeout: {} seconds)", .{ idle_seconds, idle_timeout_seconds });
        
        if (idle_seconds >= idle_timeout_seconds) {
            std.log.info("Idle timeout reached, pausing timer", .{});
            
            // Pause the timer due to idle
            self.paused_by_idle = true;
            self.timer.pause();
            
            // Start monitoring for activity to resume
            self.input_monitor.start();
            
            // Update tooltip to indicate idle pause
            self.tray_icon.setTooltip("Zigodoro - Paused (idle)") catch {};
            self.status_tray.updateTooltip("Zigodoro - Paused (idle)");
        }
        
        return 1; // G_SOURCE_CONTINUE
    }
    
    fn onInputActivityDetected(user_data: c.gpointer) callconv(.C) c.gboolean {
        const self: *Self = @ptrCast(@alignCast(user_data));
        
        std.log.info("onInputActivityDetected called!", .{});
        
        const current_state = self.timer.getState();
        std.log.info("onInputActivityDetected: current_state={}, auto_start={}, paused_by_idle={}", .{
            current_state,
            self.settings.auto_start_work_after_break,
            self.paused_by_idle,
        });
        
        // Handle auto-resume from idle pause
        if (current_state == .paused and self.paused_by_idle) {
            std.log.info("Auto-resuming work session after idle pause", .{});
            self.paused_by_idle = false;
            
            self.timer.start();
            
            // Show notification that we're resuming
            if (c.gtk_widget_get_visible(self.window) == 0) {
                c.gtk_widget_show(self.window);
                c.gtk_window_present(@ptrCast(self.window));
                c.gtk_window_set_urgency_hint(@ptrCast(self.window), 1);
            }
            
            return 0; // G_SOURCE_REMOVE
        }
        
        // Handle auto-start after break
        if (current_state == .idle and self.settings.auto_start_work_after_break) {
            std.log.info("Auto-starting work session from input activity", .{});
            
            // Ensure input monitoring is stopped to prevent multiple triggers
            self.input_monitor.stop();
            
            // Start work session
            self.timer.start();
        }
        
        return 0; // G_SOURCE_REMOVE
    }
};

// Exported C functions for D-Bus service
export fn on_start_clicked_zig(app_ptr: ?*anyopaque) void {
    if (app_ptr) |ptr| {
        const app: *ZigodoroApp = @ptrCast(@alignCast(ptr));
        const state = app.timer.getState();
        if (state == .idle or state == .paused) {
            app.timer.start();
        } else {
            app.timer.pause();
        }
    }
}

export fn on_reset_clicked_zig(app_ptr: ?*anyopaque) void {
    if (app_ptr) |ptr| {
        const app: *ZigodoroApp = @ptrCast(@alignCast(ptr));
        app.timer.reset();
    }
}

export fn timer_get_state_zig(timer_ptr: ?*anyopaque) c_int {
    if (timer_ptr) |ptr| {
        const timer: *Timer = @ptrCast(@alignCast(ptr));
        return @intFromEnum(timer.getState());
    }
    return 0;
}

export fn timer_skip_phase_zig(timer_ptr: ?*anyopaque) void {
    if (timer_ptr) |ptr| {
        const timer: *Timer = @ptrCast(@alignCast(ptr));
        timer.skipPhase();
    }
}