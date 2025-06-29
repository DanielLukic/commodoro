# GEMINI.md

This file provides guidance to Gemini when working with code in this repository.

## Project Overview

**Commodoro** is a C/GTK3 Pomodoro timer application converted from the original Python/PyQt6 Pymodoro. It features comprehensive Linux desktop integration including system tray with countdown display and progress arc, global hotkeys, break overlays, and automatic session management.

### Original Python Implementation
The original Pymodoro was a production-ready Linux-native Pomodoro timer built with PyQt6, serving as the reference for this C/GTK3 port.

## Development Commands

### C/GTK Build and Run
```bash
# Compile and run
make && ./commodoro

# Debug build
make debug && ./gomodoro

# Clean build artifacts
make clean
```

### C/GTK Dependencies
- GTK3 development libraries
- GLib/GIO for configuration and file handling  
- GtkStatusIcon for system tray integration (deprecated but functional)
- X11/Wayland development libraries for global hotkeys

### Original Python Commands (Reference)
```bash
# Normal mode (uses persistent config)
python run.py

# Test mode with custom intervals
python run.py 5s 3s 2 4s  # 5s work, 3s break, 2 sessions until long break, 4s long break
python run.py 2m 30s 2 1m  # 2m work, 30s break

# With debug logging
python run.py --log-level DEBUG
```

## Architecture

### Core Components
- **Timer System**: `timer.c`, `timer.h` - Core pomodoro logic and state management.
- **GUI Layer**: `main.c`, `settings_dialog.c`, `break_overlay.c` - Main window, system tray, break overlay, settings dialog.
- **System Tray**: `tray_icon.c`, `tray_status_icon.c` - Drawing the tray icon and integrating with the system.
- **Input Handling**: `input_monitor.c` - Global hotkeys and user activity monitoring for auto-start and idle detection.
- **Configuration**: `config.c` - Persistent and in-memory config providers.
- **Audio**: `audio.c` - Sound management for timer events.

### Key Design Patterns
- **State Machine**: A `TimerState` enum drives UI and behavior changes.
- **Callbacks**: Callbacks are used to connect components (e.g., timer state changes to GUI updates).
- **Modular Design**: Separate C files for each major component.

### Entry Point
- `main.c` - Argument parsing, configuration initialization, and app bootstrapping.

### Configuration System
- Production config: `~/.config/commodoro/config.json`
- Test mode uses in-memory config without file persistence.

### Timer Logic Flow
1. Timer state changes trigger callbacks.
2. GUI components (tray, overlay, main window) update.
3. Input monitor activates during IDLE state for auto-start or during WORK for idle detection.
4. Audio manager plays sounds based on timer events.
5. Break overlay shows during break states with a countdown.

## Development Workflow
When implementing features or fixing bugs:
1. Build and run the application (`make && ./commodoro`) to understand the current behavior.
2. Let the user confirm the functionality or bug.
3. Implement the required changes.
4. Build and run again to test the changes.
5. Create a git commit with a short summary of what was implemented.
