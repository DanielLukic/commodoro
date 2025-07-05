CC = gcc
CFLAGS_COMMON = -Wall -Wextra -std=c99
CFLAGS_GTK3 = $(CFLAGS_COMMON) $(shell pkg-config --cflags gtk+-3.0) -I./include
LIBS_GTK3 = $(shell pkg-config --libs gtk+-3.0) -lX11 -lXtst -lXi -lXss -lasound -lm -pthread
RUST_LIBS = ./target/release/libcommodoro_ffi.a
TARGET = commodoro
BUILDDIR = build
# Remove src/timer.c, src/config.c, and src/audio.c from SOURCES as we're using Rust implementations
SOURCES = src/main.c src/tray_icon.c src/tray_status_icon.c src/settings_dialog.c src/break_overlay.c src/input_monitor.c src/dbus_service.c src/dbus.c
# Remove timer.o, config.o, and audio.o from OBJECTS
OBJECTS = $(BUILDDIR)/main.o $(BUILDDIR)/tray_icon.o $(BUILDDIR)/tray_status_icon.o $(BUILDDIR)/settings_dialog.o $(BUILDDIR)/break_overlay.o $(BUILDDIR)/input_monitor.o $(BUILDDIR)/dbus_service.o $(BUILDDIR)/dbus.o

all: rust-libs $(BUILDDIR) $(TARGET)

rust-libs:
	cargo build --release

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile all sources with GTK3
$(BUILDDIR)/main.o: src/main.c
	$(CC) $(CFLAGS_GTK3) -c src/main.c -o $(BUILDDIR)/main.o

$(BUILDDIR)/tray_icon.o: src/tray_icon.c
	$(CC) $(CFLAGS_GTK3) -c src/tray_icon.c -o $(BUILDDIR)/tray_icon.o


$(BUILDDIR)/tray_status_icon.o: src/tray_status_icon.c
	$(CC) $(CFLAGS_GTK3) -c src/tray_status_icon.c -o $(BUILDDIR)/tray_status_icon.o

$(BUILDDIR)/settings_dialog.o: src/settings_dialog.c
	$(CC) $(CFLAGS_GTK3) -c src/settings_dialog.c -o $(BUILDDIR)/settings_dialog.o

$(BUILDDIR)/break_overlay.o: src/break_overlay.c
	$(CC) $(CFLAGS_GTK3) -c src/break_overlay.c -o $(BUILDDIR)/break_overlay.o

$(BUILDDIR)/input_monitor.o: src/input_monitor.c
	$(CC) $(CFLAGS_GTK3) -c src/input_monitor.c -o $(BUILDDIR)/input_monitor.o

$(BUILDDIR)/dbus_service.o: src/dbus_service.c
	$(CC) $(CFLAGS_GTK3) -c src/dbus_service.c -o $(BUILDDIR)/dbus_service.o

$(BUILDDIR)/dbus.o: src/dbus.c
	$(CC) $(CFLAGS_GTK3) -c src/dbus.c -o $(BUILDDIR)/dbus.o

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