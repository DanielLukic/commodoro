# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Commodoro** is a C/GTK3 Pomodoro timer application converted from the original Python/PyQt6 Pymodoro. It features comprehensive Linux desktop integration including system tray with countdown display and progress arc, global hotkeys, break overlays, and automatic session management.

### Original Python Implementation
The original Pymodoro was a production-ready Linux-native Pomodoro timer built with PyQt6, serving as the reference for this C/GTK4 port.

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
- **Timer System**: `timer/` - Core pomodoro logic (core.py), state management (states.py), and controller (controller.py)
- **GUI Layer**: `gui/` - Main window, system tray, break overlay, settings dialog, and styling
- **Input Handling**: `input/` - Global hotkeys and user activity monitoring for auto-start
- **Configuration**: `config/` - Persistent and in-memory config providers using Pydantic models
- **Audio**: `audio/` - Sound management for timer events
- **Utils**: `utils/` - Logging utilities

### Key Design Patterns
- **Provider Pattern**: ConfigProvider with pluggable backends (PersistentConfig for normal mode, InMemoryConfig for test mode)
- **Signal-Slot Architecture**: PyQt6 signals connect timer state changes to GUI updates, audio events, and input monitoring
- **State Machine**: TimerState enum drives all UI and behavior changes
- **Component Lifecycle**: Clean initialization/cleanup pattern in PyomodoroApp

### C/GTK Implementation Mapping
- **GObject System**: Use GObject for signal/slot architecture mimicking PyQt6
- **Event-Driven**: GTK main loop handles timer events and GUI updates
- **State Machine**: Enum-based state management with signal emissions
- **Modular Design**: Separate compilation units for each major component

### Entry Points
- `run.py` - Simple entry point that imports from `pymodoro.main`
- `pymodoro/main.py` - Argument parsing, config initialization, and app bootstrapping
- `pymodoro/app.py` - Main application class coordinating all components

### Configuration System
- Production config: `~/.config/pymodoro/config.json`
- State persistence: `~/.config/pymodoro/state.json`
- Logs: `~/.config/pymodoro/logs/`
- Test mode uses in-memory config without file persistence

### Timer Logic Flow
1. Timer state changes trigger signals
2. GUI components (tray, overlay, main window) update automatically
3. Input monitor activates during IDLE state for auto-start
4. Audio manager plays sounds based on timer events
5. Break overlay shows during break states with countdown

## Python to C/GTK Conversion Plan

### Python Dependencies Analysis
- **PyQt6**: GUI framework → **GTK3** (widgets, windows, signals)
- **pynput**: Global hotkeys and input monitoring → **X11/XInput2** or **libinput**
- **pydantic**: Data validation and settings → **GLib GKeyFile** or **JSON-GLib**
- **pydantic-settings**: Config management → **GSettings** or custom config system
- **pygame/pyaudio**: Audio playback → **GStreamer** or **PulseAudio**

### UI Design Requirements
- **Circular Tray Icon**: Red tomato shape with green border that fills as timer progresses
- **Dark Theme**: Match the dark UI shown in doc/pomowin.png
- **Main Window**: Large timer display, session info, control buttons, settings section
- **Break Overlay**: Fullscreen countdown during breaks

### Component Conversion Steps
1. **Timer Core**: Convert Python timer logic to C with GLib timers ✅
2. **GTK3 UI**: Build main window matching the Python version's layout ✅
3. **Tray Icon**: Create circular tomato icon with progress ring using Cairo ✅
4. **System Tray**: Pure GTK3 solution using GtkStatusIcon (single executable) ✅
5. **GTK3 Conversion**: Resolved GTK version mixing by converting entire app to GTK3 ✅
6. **Audio System**: Integrate GStreamer for timer sounds ✅
7. **Settings Dialog**: 3-tab interface (Timer/Misc/Sounds) with full configuration ✅
8. **Tray Icon Text**: Add remaining time display inside circular icon
9. **Break Overlay**: Full-screen GTK window for break periods  
10. **Auto-progression**: Work→break→work automatic transitions
11. **Session Management**: Long break after configured sessions
12. **Configuration Persistence**: Save/load settings from ~/.config/commodoro/
13. **Enhanced Audio**: Sophisticated chimes with ADSR envelopes and custom file support

### Development Workflow
After each work package:
1. Build and run the application (`make && ./commodoro`)
2. Let user confirm functionality works as expected
3. Create git commit with short summary of what was implemented
4. Move to next work package