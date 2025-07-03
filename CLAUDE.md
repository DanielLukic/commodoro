# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
- **Timer System**: `src/timer/` - Core pomodoro logic and state management
- **GUI Layer**: `src/gui/` - Main window, system tray, break overlay, settings dialog, and styling
- **Input Handling**: `src/input/` - Global hotkeys and user activity monitoring for auto-start
- **Configuration**: `src/config/` - Persistent and in-memory config providers using JSON
- **Audio**: `src/audio/` - Sound management for timer events
- **D-Bus**: `src/dbus/` - D-Bus integration for command-line control

### Key Design Patterns
- **Memory Safety**: Zig's built-in memory safety and error handling
- **Callback Architecture**: Timer state changes trigger callbacks to GUI updates, audio events, and input monitoring
- **State Machine**: TimerState enum drives all UI and behavior changes
- **Component Lifecycle**: Clean initialization/cleanup pattern in ZigodoroApp

### Zig/GTK Architecture
- **GTK Integration**: Direct GTK3 bindings through Zig's C interop
- **Event-Driven**: GTK main loop handles timer events and GUI updates
- **State Machine**: Enum-based state management with callback emissions
- **Modular Design**: Separate Zig files for each major component

### Entry Points
- `src/main.zig` - Main entry point with argument parsing and app bootstrapping
- `src/app.zig` - Main application class coordinating all components

### Configuration System
- Production config: `~/.config/zigodoro/config.json`
- State persistence: `~/.config/zigodoro/state.json`
- Test mode uses in-memory config without file persistence

### Timer Logic Flow
1. Timer state changes trigger signals
2. GUI components (tray, overlay, main window) update automatically
3. Input monitor activates during IDLE state for auto-start
4. Audio manager plays sounds based on timer events
5. Break overlay shows during break states with countdown

## Implementation Status

### Technology Stack
- **Zig**: System programming language with memory safety
- **GTK3**: GUI framework for Linux desktop integration
- **Cairo**: Graphics library for custom tray icon rendering
- **X11/XInput2**: Input monitoring and idle detection
- **ALSA**: Audio playback for timer sounds
- **D-Bus**: Inter-process communication for command-line control

### UI Design Requirements
- **Circular Tray Icon**: Red tomato shape with green border that fills as timer progresses
- **Dark Theme**: Match the dark UI shown in doc/pomowin.png
- **Main Window**: Large timer display, session info, control buttons, settings section
- **Break Overlay**: Fullscreen countdown during breaks

### Implementation Progress
1. **Timer Core**: Core pomodoro logic with Zig timers ✅
2. **GTK3 UI**: Main window with timer display and controls ✅
3. **Tray Icon**: Circular icon with progress ring using Cairo ✅
4. **System Tray**: Pure GTK3 solution using GtkStatusIcon (single executable) ✅
5. **GTK3 Conversion**: Resolved GTK version mixing by converting entire app to GTK3 ✅
6. **Audio System**: Integrate GStreamer for timer sounds ✅
7. **Settings Dialog**: 3-tab interface (Timer/Misc/Sounds) with full configuration ✅
8. **Tray Icon Text**: Add remaining time display inside circular icon
9. **Break Overlay**: Full-screen GTK window for break periods  
10. **Auto-progression**: Work→break→work automatic transitions
11. **Session Management**: Long break after configured sessions
12. **Configuration Persistence**: Save/load settings from ~/.config/zigodoro/
13. **Enhanced Audio**: ALSA-based audio with timer event sounds

### Development Workflow
After each work package:
1. Build and run the application (`zig build run`)
2. Let user confirm functionality works as expected
3. Create git commit with short summary of what was implemented
4. Move to next work package