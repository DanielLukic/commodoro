const std = @import("std");

// External C functions
extern fn dbus_service_new(app_pointer: ?*anyopaque) ?*anyopaque;
extern fn dbus_service_free(service: *anyopaque) void;
extern fn dbus_service_publish(service: *anyopaque) void;
extern fn dbus_service_unpublish(service: *anyopaque) void;

pub const DBusService = struct {
    service: *anyopaque,
    allocator: std.mem.Allocator,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator, app_pointer: ?*anyopaque) !*Self {
        const self = try allocator.create(Self);
        errdefer allocator.destroy(self);
        
        const service = dbus_service_new(app_pointer) orelse return error.DBusServiceCreationFailed;
        
        self.* = .{
            .allocator = allocator,
            .service = service,
        };
        
        return self;
    }
    
    pub fn deinit(self: *Self) void {
        dbus_service_free(self.service);
        self.allocator.destroy(self);
    }
    
    pub fn publish(self: *Self) void {
        dbus_service_publish(self.service);
    }
    
    pub fn unpublish(self: *Self) void {
        dbus_service_unpublish(self.service);
    }
};