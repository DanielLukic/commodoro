CC = gcc
CFLAGS_COMMON = -Wall -Wextra -std=c99
CFLAGS_GTK3 = $(CFLAGS_COMMON) $(shell pkg-config --cflags gtk+-3.0) -I./include
LIBS_GTK3 = $(shell pkg-config --libs gtk+-3.0) -lX11 -lXtst -lXi -lXss -lasound -lm -pthread
RUST_LIBS = ./target/release/libcommodoro_ffi.a
TARGET = commodoro
BUILDDIR = build
# Remove src/timer.c, src/config.c, src/audio.c, src/tray_icon.c, src/break_overlay.c, src/settings_dialog.c, src/dbus_service.c, and src/input_monitor.c from SOURCES as we're using Rust implementations
SOURCES = src/main.c src/dbus.c src/tray_status_icon.c
# Remove timer.o, config.o, audio.o, tray_icon.o, break_overlay.o, settings_dialog.o, dbus_service.o, and input_monitor.o from OBJECTS
OBJECTS = $(BUILDDIR)/main.o $(BUILDDIR)/dbus.o $(BUILDDIR)/tray_status_icon.o

all: rust-libs $(BUILDDIR) $(TARGET)

rust-libs:
	cargo build --release

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile all sources with GTK3
$(BUILDDIR)/main.o: src/main.c
	$(CC) $(CFLAGS_GTK3) -c src/main.c -o $(BUILDDIR)/main.o

# Removed input_monitor.c compilation - using Rust implementation
# Removed dbus_service.c compilation - using Rust implementation

$(BUILDDIR)/dbus.o: src/dbus.c
	$(CC) $(CFLAGS_GTK3) -c src/dbus.c -o $(BUILDDIR)/dbus.o

$(BUILDDIR)/tray_status_icon.o: src/tray_status_icon.c
	$(CC) $(CFLAGS_GTK3) -c src/tray_status_icon.c -o $(BUILDDIR)/tray_status_icon.o

# Link everything together with Rust libraries
$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(RUST_LIBS) $(LIBS_GTK3)

debug: CFLAGS_COMMON += -g -DDEBUG
debug: $(TARGET)

clean:
	rm -rf $(BUILDDIR) $(TARGET)
	cargo clean

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all debug clean install