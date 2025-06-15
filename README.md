# Commodoro

A fast, native C/GTK3 Pomodoro timer for Linux with comprehensive desktop integration.

## Features

- **Pomodoro Timer**: 25-minute work sessions with 5-minute breaks
- **System Tray**: Live countdown with progress arc and state colors
- **Break Overlays**: Full-screen break notifications 
- **Auto-Start**: Detects user activity to start work sessions
- **Dark Theme**: Modern GTK3 interface optimized for focus
- **Audio Alerts**: Built-in chimes with enable/disable toggle
- **Configuration**: Persistent settings in `~/.config/commodoro/`

## Quick Start

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install libgtk-3-dev libgstreamer1.0-dev libxtst-dev

# Build and run
make clean && make
./commodoro
```

## Usage

```bash
./commodoro                    # Standard 25/5/15 minute intervals
./commodoro 5s 3s 2 4s        # Test mode: 5s work, 3s break, 2 sessions, 4s long break
./commodoro --log-level DEBUG # Debug logging
```

**Time units**: `s` (seconds), `m` (minutes), `h` (hours), no suffix = minutes

## System Tray

- **Gray (●)**: Ready/Idle
- **Red**: Work session with countdown
- **Teal**: Short break with countdown  
- **Blue**: Long break with countdown
- **Yellow (||)**: Paused
- **Progress Arc**: Fills clockwise during sessions

## Keyboard Shortcuts

- **Space**: Start/Pause timer
- **Escape**: Hide to system tray
- **Settings Button**: Open configuration

## Audio Features

### Current (v1)
- ✅ Enable/disable sounds
- ✅ Built-in chimes (different frequencies per event)
- ✅ Fixed volume (30%)

### Planned (v2)
- Volume control slider with real-time testing
- Custom sound file support (.wav, .mp3, .ogg)
- Per-event sound customization (work start, break start, session complete)
- Enhanced settings dialog with dedicated Sounds tab
- Sound file validation and preview

## Architecture

**Core Components**: Timer state machine, GTK3 GUI, system tray integration, GStreamer audio, input monitoring, persistent configuration

**Clean C99**: Modular design with proper memory management and error handling

## Contributing

See `CLAUDE.md` for development guidelines and project structure.
