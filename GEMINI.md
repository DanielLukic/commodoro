# GEMINI.md

This file provides guidance to Gemini when working with code in this repository.

## Project Overview

**Zigodoro** is a Zig/GTK3 Pomodoro timer application. It features comprehensive Linux desktop integration including system tray with countdown display and progress arc, global hotkeys, break overlays, and automatic session management.


## Development Commands

### Zig/GTK Build and Run
```bash
# Compile and run
zig build run

# Debug build
zig build -Doptimize=Debug && ./zig-out/bin/zigodoro

# Clean build artifacts
rm -rf zig-out .zig-cache
```

### Zig/GTK Dependencies
- GTK3 development libraries
- GLib/GIO for configuration and file handling  
- GtkStatusIcon for system tray integration (deprecated but functional)
- X11/Wayland development libraries for global hotkeys


## Architecture

### Core Components
- **Timer System**: `src/timer/timer.zig` - Core pomodoro logic and state management.
- **GUI Layer**: `src/app.zig`, `src/gui/` - Main window, system tray, break overlay, settings dialog.
- **System Tray**: `src/gui/tray_icon.zig`, `src/gui/tray_status_icon.zig` - Drawing the tray icon and integrating with the system.
- **Input Handling**: `src/input/input_monitor.zig` - Global hotkeys and user activity monitoring for auto-start and idle detection.
- **Configuration**: `src/config/config_manager.zig` - Persistent and in-memory config providers.
- **Audio**: `src/audio/audio.zig` - Sound management for timer events.

### Key Design Patterns
- **State Machine**: A `TimerState` enum drives UI and behavior changes.
- **Callbacks**: Callbacks are used to connect components (e.g., timer state changes to GUI updates).
- **Modular Design**: Separate C files for each major component.

### Entry Point
- `src/main.zig` - Argument parsing, configuration initialization, and app bootstrapping.

### Configuration System
- Production config: `~/.config/zigodoro/config.json`
- Test mode uses in-memory config without file persistence.

### Timer Logic Flow
1. Timer state changes trigger callbacks.
2. GUI components (tray, overlay, main window) update.
3. Input monitor activates during IDLE state for auto-start or during WORK for idle detection.
4. Audio manager plays sounds based on timer events.
5. Break overlay shows during break states with a countdown.

## Development Workflow
When implementing features or fixing bugs:
1. Build and run the application (`zig build run`) to understand the current behavior.
2. Let the user confirm the functionality or bug.
3. Implement the required changes.
4. Build and run again to test the changes.
5. Create a git commit with a short summary of what was implemented.
