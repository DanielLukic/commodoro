// Minimal GTK bindings for Zig to avoid GdkEvent issues
// This bypasses the problematic cImport and defines only what we need

const std = @import("std");

// Basic type definitions
pub const gboolean = c_int;
pub const gint = c_int;
pub const guint = c_uint;
pub const gpointer = ?*anyopaque;
pub const gchar = u8;

// GTK enums we need
pub const GtkWindowType = enum(c_uint) {
    GTK_WINDOW_TOPLEVEL = 0,
    GTK_WINDOW_POPUP = 1,
};

pub const GtkOrientation = enum(c_uint) {
    GTK_ORIENTATION_HORIZONTAL = 0,
    GTK_ORIENTATION_VERTICAL = 1,
};

pub const GtkAlign = enum(c_uint) {
    GTK_ALIGN_FILL = 0,
    GTK_ALIGN_START = 1,
    GTK_ALIGN_END = 2,
    GTK_ALIGN_CENTER = 3,
    GTK_ALIGN_BASELINE = 4,
};

pub const GtkWindowPosition = enum(c_uint) {
    GTK_WIN_POS_NONE = 0,
    GTK_WIN_POS_CENTER = 1,
    GTK_WIN_POS_MOUSE = 2,
    GTK_WIN_POS_CENTER_ALWAYS = 3,
    GTK_WIN_POS_CENTER_ON_PARENT = 4,
};

// Opaque types
pub const GtkWidget = opaque {};
pub const GtkWindow = opaque {};
pub const GtkBox = opaque {};
pub const GtkLabel = opaque {};
pub const GtkButton = opaque {};
pub const GtkContainer = opaque {};
pub const GtkStatusIcon = opaque {};
pub const GtkMenu = opaque {};
pub const GtkMenuItem = opaque {};
pub const GtkMenuShell = opaque {};
pub const GtkCssProvider = opaque {};
pub const GtkStyleContext = opaque {};
pub const GtkStyleProvider = opaque {};
pub const GdkScreen = opaque {};
pub const GObject = opaque {};
pub const GtkAccelGroup = opaque {};
pub const GdkDisplay = opaque {};
pub const GdkMonitor = opaque {};
pub const GtkDialog = opaque {};
pub const GtkNotebook = opaque {};
pub const GtkSpinButton = opaque {};
pub const GtkCheckButton = opaque {};
pub const GtkToggleButton = opaque {};
pub const GtkFrame = opaque {};
pub const GtkGrid = opaque {};
pub const GdkPixbuf = opaque {};
pub const GdkEventKey = extern struct {
    type: c_int,
    window: *anyopaque,
    send_event: gint,
    time: guint,
    state: guint,
    keyval: guint,
    length: gint,
    string: [*c]gchar,
    hardware_keycode: guint,
    group: guint,
    is_modifier: guint,
};
pub const GdkRectangle = extern struct {
    x: gint,
    y: gint,
    width: gint,
    height: gint,
};

// Constants
pub const TRUE: gboolean = 1;
pub const FALSE: gboolean = 0;
pub const GTK_STYLE_PROVIDER_PRIORITY_APPLICATION: c_uint = 600;

// Function declarations
pub extern fn gtk_init(argc: ?*c_int, argv: ?*[*][*:0]u8) void;
pub extern fn gtk_main() void;
pub extern fn gtk_main_quit() void;

// Window functions
pub extern fn gtk_window_new(type: c_uint) ?*GtkWidget;
pub extern fn gtk_window_set_title(window: *GtkWindow, title: [*:0]const u8) void;
pub extern fn gtk_window_set_default_size(window: *GtkWindow, width: gint, height: gint) void;
pub extern fn gtk_window_set_resizable(window: *GtkWindow, resizable: gboolean) void;
pub extern fn gtk_window_set_position(window: *GtkWindow, position: c_uint) void;
pub extern fn gtk_window_present(window: *GtkWindow) void;
pub extern fn gtk_window_set_urgency_hint(window: *GtkWindow, setting: gboolean) void;
pub extern fn gtk_window_set_decorated(window: *GtkWindow, setting: gboolean) void;
pub extern fn gtk_window_set_skip_taskbar_hint(window: *GtkWindow, setting: gboolean) void;
pub extern fn gtk_window_set_skip_pager_hint(window: *GtkWindow, setting: gboolean) void;
pub extern fn gtk_window_set_keep_above(window: *GtkWindow, setting: gboolean) void;
pub extern fn gtk_window_stick(window: *GtkWindow) void;
pub extern fn gtk_window_fullscreen(window: *GtkWindow) void;
pub extern fn gtk_window_move(window: *GtkWindow, x: gint, y: gint) void;
pub extern fn gtk_window_set_transient_for(window: *GtkWindow, parent: *GtkWindow) void;
pub extern fn gtk_window_set_modal(window: *GtkWindow, modal: gboolean) void;

// Widget functions
pub extern fn gtk_widget_show(widget: *GtkWidget) void;
pub extern fn gtk_widget_hide(widget: *GtkWidget) void;
pub extern fn gtk_widget_show_all(widget: *GtkWidget) void;
pub extern fn gtk_widget_get_visible(widget: *GtkWidget) gboolean;
pub extern fn gtk_widget_set_halign(widget: *GtkWidget, alignment: c_uint) void;
pub extern fn gtk_widget_set_valign(widget: *GtkWidget, alignment: c_uint) void;
pub extern fn gtk_widget_set_margin_top(widget: *GtkWidget, margin: gint) void;
pub extern fn gtk_widget_set_margin_bottom(widget: *GtkWidget, margin: gint) void;
pub extern fn gtk_widget_set_margin_start(widget: *GtkWidget, margin: gint) void;
pub extern fn gtk_widget_set_margin_end(widget: *GtkWidget, margin: gint) void;
pub extern fn gtk_widget_get_style_context(widget: *GtkWidget) *GtkStyleContext;
pub extern fn gtk_widget_set_sensitive(widget: *GtkWidget, sensitive: gboolean) void;
pub extern fn gtk_widget_set_size_request(widget: *GtkWidget, width: gint, height: gint) void;
pub extern fn gtk_widget_grab_focus(widget: *GtkWidget) void;
pub extern fn gtk_widget_destroy(widget: *GtkWidget) void;

// Container functions
pub extern fn gtk_container_add(container: *GtkContainer, widget: *GtkWidget) void;

// Box functions
pub extern fn gtk_box_new(orientation: c_uint, spacing: gint) ?*GtkWidget;
pub extern fn gtk_box_pack_start(box: *GtkBox, child: *GtkWidget, expand: gboolean, fill: gboolean, padding: guint) void;

// Label functions
pub extern fn gtk_label_new(str: [*:0]const u8) ?*GtkWidget;
pub extern fn gtk_label_set_text(label: *GtkLabel, str: [*:0]const u8) void;

// Button functions
pub extern fn gtk_button_new_with_label(label: [*:0]const u8) ?*GtkWidget;
pub extern fn gtk_button_set_label(button: *GtkButton, label: [*:0]const u8) void;

// CSS functions
pub extern fn gtk_css_provider_new() *GtkCssProvider;
pub extern fn gtk_css_provider_load_from_data(css_provider: *GtkCssProvider, data: [*]const u8, length: isize, err: ?*anyopaque) gboolean;
pub extern fn gtk_style_context_add_class(context: *GtkStyleContext, class_name: [*:0]const u8) void;
pub extern fn gtk_style_context_remove_class(context: *GtkStyleContext, class_name: [*:0]const u8) void;
pub extern fn gtk_style_context_add_provider_for_screen(screen: *GdkScreen, provider: *GtkStyleProvider, priority: guint) void;
pub extern fn gdk_screen_get_default() *GdkScreen;

// GDK Display/Monitor functions
pub extern fn gdk_display_get_default() *GdkDisplay;
pub extern fn gdk_display_get_n_monitors(display: *GdkDisplay) c_int;
pub extern fn gdk_display_get_primary_monitor(display: *GdkDisplay) ?*GdkMonitor;
pub extern fn gdk_display_get_monitor(display: *GdkDisplay, monitor_num: c_int) *GdkMonitor;
pub extern fn gdk_monitor_get_geometry(monitor: *GdkMonitor, geometry: *GdkRectangle) void;

// Dialog functions
pub extern fn gtk_dialog_new() ?*GtkWidget;
pub extern fn gtk_dialog_get_content_area(dialog: *GtkDialog) *GtkWidget;
pub extern fn gtk_dialog_get_action_area(dialog: *GtkDialog) *GtkWidget;

// Notebook functions
pub extern fn gtk_notebook_new() ?*GtkWidget;
pub extern fn gtk_notebook_append_page(notebook: *GtkNotebook, child: *GtkWidget, tab_label: *GtkWidget) gint;

// SpinButton functions
pub extern fn gtk_spin_button_new_with_range(min: f64, max: f64, step: f64) ?*GtkWidget;
pub extern fn gtk_spin_button_set_value(spin_button: *GtkSpinButton, value: f64) void;
pub extern fn gtk_spin_button_get_value(spin_button: *GtkSpinButton) f64;

// CheckButton/ToggleButton functions
pub extern fn gtk_check_button_new_with_label(label: [*:0]const u8) ?*GtkWidget;
pub extern fn gtk_toggle_button_set_active(toggle_button: *GtkToggleButton, is_active: gboolean) void;
pub extern fn gtk_toggle_button_get_active(toggle_button: *GtkToggleButton) gboolean;

// Frame functions
pub extern fn gtk_frame_new(label: [*:0]const u8) ?*GtkWidget;

// Grid functions
pub extern fn gtk_grid_new() ?*GtkWidget;
pub extern fn gtk_grid_set_row_spacing(grid: *GtkGrid, spacing: guint) void;
pub extern fn gtk_grid_set_column_spacing(grid: *GtkGrid, spacing: guint) void;
pub extern fn gtk_grid_attach(grid: *GtkGrid, child: *GtkWidget, left: gint, top: gint, width: gint, height: gint) void;

// Container border width
pub extern fn gtk_container_set_border_width(container: *GtkContainer, border_width: guint) void;

// Widget margin functions
pub extern fn gtk_widget_set_margin_left(widget: *GtkWidget, margin: gint) void;

// StatusIcon functions
pub extern fn gtk_status_icon_new() ?*GtkStatusIcon;
pub extern fn gtk_status_icon_set_title(status_icon: *GtkStatusIcon, title: [*:0]const u8) void;
pub extern fn gtk_status_icon_set_tooltip_text(status_icon: *GtkStatusIcon, text: [*:0]const u8) void;
pub extern fn gtk_status_icon_set_visible(status_icon: *GtkStatusIcon, visible: gboolean) void;
pub extern fn gtk_status_icon_is_embedded(status_icon: *GtkStatusIcon) gboolean;
pub extern fn gtk_status_icon_set_from_icon_name(status_icon: *GtkStatusIcon, icon_name: [*:0]const u8) void;
pub extern fn gtk_status_icon_set_from_pixbuf(status_icon: *GtkStatusIcon, pixbuf: ?*GdkPixbuf) void;

// Menu functions
pub extern fn gtk_menu_new() *GtkMenu;
pub extern fn gtk_menu_popup_at_pointer(menu: *GtkMenu, trigger_event: ?*anyopaque) void;
pub extern fn gtk_menu_item_new_with_label(label: [*:0]const u8) *GtkMenuItem;
pub extern fn gtk_separator_menu_item_new() *GtkWidget;
pub extern fn gtk_menu_shell_append(menu_shell: *GtkMenuShell, child: *GtkWidget) void;

// GObject functions
pub extern fn g_object_ref(object: *anyopaque) *anyopaque;
pub extern fn g_object_unref(object: *anyopaque) void;
pub extern fn g_signal_connect_data(instance: *anyopaque, detailed_signal: [*:0]const u8, c_handler: *const anyopaque, data: ?*anyopaque, destroy_data: ?*const anyopaque, connect_flags: c_uint) c_ulong;
pub extern fn g_cclosure_new(callback_func: *const anyopaque, user_data: ?*anyopaque, destroy_data: ?*const anyopaque) *anyopaque;

// GLib main loop functions
pub extern fn g_timeout_add_seconds(interval: guint, function: *const fn (gpointer) callconv(.C) gboolean, data: gpointer) guint;
pub extern fn g_source_remove(tag: guint) gboolean;

// Accelerator group
pub extern fn gtk_accel_group_new() *GtkAccelGroup;
pub extern fn gtk_window_add_accel_group(window: *GtkWindow, accel_group: *GtkAccelGroup) void;
pub extern fn gtk_accel_group_connect(accel_group: *GtkAccelGroup, accel_key: guint, accel_mods: c_uint, accel_flags: c_uint, closure: *anyopaque) void;

// Key constants
pub const GDK_CONTROL_MASK: c_uint = 1 << 2;
pub const GTK_ACCEL_VISIBLE: c_uint = 1 << 0;
pub const GDK_KEY_Escape: guint = 0xff1b;
pub const GDK_KEY_s: guint = 0x0073;
pub const GDK_KEY_S: guint = 0x0053;
pub const GDK_KEY_e: guint = 0x0065;
pub const GDK_KEY_E: guint = 0x0045;
pub const GDK_KEY_p: guint = 0x0070;
pub const GDK_KEY_P: guint = 0x0050;
pub const GDK_KEY_r: guint = 0x0072;
pub const GDK_KEY_R: guint = 0x0052;

// Cairo functions
pub extern fn cairo_image_surface_get_width(surface: *anyopaque) c_int;
pub extern fn cairo_image_surface_get_height(surface: *anyopaque) c_int;

// GdkPixbuf functions
pub extern fn gdk_pixbuf_get_from_surface(surface: *anyopaque, src_x: gint, src_y: gint, width: gint, height: gint) ?*GdkPixbuf;