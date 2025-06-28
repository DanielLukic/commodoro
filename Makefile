CC = gcc
CFLAGS_COMMON = -Wall -Wextra -std=c99
CFLAGS_GTK3 = $(CFLAGS_COMMON) $(shell pkg-config --cflags gtk+-3.0)
LIBS_GTK3 = $(shell pkg-config --libs gtk+-3.0) -lX11 -lXtst -lXi -lXss -lasound -lm -pthread
TARGET = commodoro
BUILDDIR = build
SOURCES = src/main.c src/tray_icon.c src/timer.c src/tray_status_icon.c src/audio.c src/settings_dialog.c src/break_overlay.c src/config.c src/input_monitor.c
OBJECTS = $(BUILDDIR)/main.o $(BUILDDIR)/tray_icon.o $(BUILDDIR)/timer.o $(BUILDDIR)/tray_status_icon.o $(BUILDDIR)/audio.o $(BUILDDIR)/settings_dialog.o $(BUILDDIR)/break_overlay.o $(BUILDDIR)/config.o $(BUILDDIR)/input_monitor.o

all: $(BUILDDIR) $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile all sources with GTK3
$(BUILDDIR)/main.o: src/main.c
	$(CC) $(CFLAGS_GTK3) -c src/main.c -o $(BUILDDIR)/main.o

$(BUILDDIR)/tray_icon.o: src/tray_icon.c
	$(CC) $(CFLAGS_GTK3) -c src/tray_icon.c -o $(BUILDDIR)/tray_icon.o

$(BUILDDIR)/timer.o: src/timer.c
	$(CC) $(CFLAGS_GTK3) -c src/timer.c -o $(BUILDDIR)/timer.o

$(BUILDDIR)/tray_status_icon.o: src/tray_status_icon.c
	$(CC) $(CFLAGS_GTK3) -c src/tray_status_icon.c -o $(BUILDDIR)/tray_status_icon.o

$(BUILDDIR)/audio.o: src/audio.c
	$(CC) $(CFLAGS_GTK3) -c src/audio.c -o $(BUILDDIR)/audio.o

$(BUILDDIR)/settings_dialog.o: src/settings_dialog.c
	$(CC) $(CFLAGS_GTK3) -c src/settings_dialog.c -o $(BUILDDIR)/settings_dialog.o

$(BUILDDIR)/break_overlay.o: src/break_overlay.c
	$(CC) $(CFLAGS_GTK3) -c src/break_overlay.c -o $(BUILDDIR)/break_overlay.o

$(BUILDDIR)/config.o: src/config.c
	$(CC) $(CFLAGS_GTK3) -c src/config.c -o $(BUILDDIR)/config.o

$(BUILDDIR)/input_monitor.o: src/input_monitor.c
	$(CC) $(CFLAGS_GTK3) -c src/input_monitor.c -o $(BUILDDIR)/input_monitor.o

# Link everything together
$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LIBS_GTK3)

debug: CFLAGS_COMMON += -g -DDEBUG
debug: $(TARGET)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all debug clean install