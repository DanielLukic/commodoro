CC = gcc
CFLAGS_COMMON = -Wall -Wextra -std=c99
CFLAGS_GTK3 = $(CFLAGS_COMMON) $(shell pkg-config --cflags gtk+-3.0 gstreamer-1.0)
LIBS_GTK3 = $(shell pkg-config --libs gtk+-3.0 gstreamer-1.0) -lX11 -lXtst -pthread
TARGET = commodoro
SOURCES = src/main.c src/tray_icon.c src/timer.c src/tray_status_icon.c src/audio.c src/settings_dialog.c src/break_overlay.c src/config.c src/input_monitor.c

all: $(TARGET)

# Compile all sources with GTK3
main.o: src/main.c
	$(CC) $(CFLAGS_GTK3) -c src/main.c -o main.o

tray_icon.o: src/tray_icon.c
	$(CC) $(CFLAGS_GTK3) -c src/tray_icon.c -o tray_icon.o

timer.o: src/timer.c
	$(CC) $(CFLAGS_GTK3) -c src/timer.c -o timer.o

tray_status_icon.o: src/tray_status_icon.c
	$(CC) $(CFLAGS_GTK3) -c src/tray_status_icon.c -o tray_status_icon.o

audio.o: src/audio.c
	$(CC) $(CFLAGS_GTK3) -c src/audio.c -o audio.o

settings_dialog.o: src/settings_dialog.c
	$(CC) $(CFLAGS_GTK3) -c src/settings_dialog.c -o settings_dialog.o

break_overlay.o: src/break_overlay.c
	$(CC) $(CFLAGS_GTK3) -c src/break_overlay.c -o break_overlay.o

config.o: src/config.c
	$(CC) $(CFLAGS_GTK3) -c src/config.c -o config.o

input_monitor.o: src/input_monitor.c
	$(CC) $(CFLAGS_GTK3) -c src/input_monitor.c -o input_monitor.o

# Link everything together
$(TARGET): main.o tray_icon.o timer.o tray_status_icon.o audio.o settings_dialog.o break_overlay.o config.o input_monitor.o
	$(CC) -o $(TARGET) main.o tray_icon.o timer.o tray_status_icon.o audio.o settings_dialog.o break_overlay.o config.o input_monitor.o $(LIBS_GTK3)

debug: CFLAGS_COMMON += -g -DDEBUG
debug: $(TARGET)

clean:
	rm -f $(TARGET) *.o

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all debug clean install