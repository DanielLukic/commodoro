# Rust Port Plan for Commodoro

## Overview

This document outlines a plan for incrementally porting the Commodoro C/GTK3 Pomodoro timer application to Rust. We use FFI (Foreign Function Interface) as a **transitional strategy** to maintain compatibility during migration, with the goal of achieving a pure Rust application where feasible.

## Goals

1. **Incremental Migration**: Port one module at a time while keeping the application functional
2. **Transitional FFI**: Use C ABI temporarily to enable gradual migration
3. **Pure Rust Target**: Aim for a fully Rust application, keeping C modules via FFI only where necessary
4. **Improve Safety**: Leverage Rust's memory safety and type system
5. **Preserve Features**: Maintain all existing functionality during migration
6. **Modern Stack**: Use idiomatic Rust crates (gtk-rs, serde, tokio, etc.) instead of C libraries

## Architecture Strategy

### Migration Philosophy

**FFI is a means, not an end**: We use FFI to enable incremental migration while keeping the app functional. The end goal is to eliminate FFI boundaries where possible, resulting in a pure Rust application with C modules via FFI only where absolutely necessary.

### Transitional FFI Approach

1. **Phase 1 - Rust with C FFI**: Create Rust modules exposing C-compatible interfaces
2. **Phase 2 - Dual Operation**: Run C and Rust modules side-by-side via FFI
3. **Phase 3 - Rust Dependencies**: Replace C dependencies with Rust crates (gtk-rs instead of GTK C)
4. **Phase 4 - Pure Rust**: Remove FFI layers as modules become fully Rust
5. **Phase 5 - Selective FFI**: Keep C modules only where Rust alternatives are inadequate

### Module Dependencies

```
┌─────────────┐
│    main     │ (Last to port)
└──────┬──────┘
       │
┌──────▼──────┐     ┌─────────────┐     ┌──────────────┐
│ GomodaroApp │────►│ TrayStatusIcon│────►│  TrayIcon    │
└──────┬──────┘     └─────────────┘     └──────────────┘
       │                                         │
       │            ┌─────────────┐              │
       ├───────────►│   Timer     │◄─────────────┘
       │            └──────┬──────┘
       │                   │
       │            ┌──────▼──────┐     ┌──────────────┐
       ├───────────►│   Audio     │     │   Config     │◄─┐
       │            └─────────────┘     └──────────────┘  │
       │                                                   │
       │            ┌─────────────┐     ┌──────────────┐  │
       ├───────────►│BreakOverlay │     │InputMonitor  │  │
       │            └─────────────┘     └──────────────┘  │
       │                                                   │
       │            ┌─────────────┐     ┌──────────────┐  │
       └───────────►│   D-Bus     │     │SettingsDialog├──┘
                    └─────────────┘     └──────────────┘
```

## Phased Migration Plan

### Phase 1: Core Logic Modules (Weeks 1-2)

These modules have minimal UI dependencies and clear interfaces.

#### 1.1 Timer Module
- **Priority**: HIGH - Core business logic
- **Dependencies**: None (pure logic)
- **Rust Benefits**: State machine pattern, thread safety
- **FFI Approach** (Transitional):
  ```rust
  // Initial: Expose C FFI for existing C code
  #[no_mangle]
  pub extern "C" fn timer_new() -> *mut Timer { ... }
  ```
- **Final Rust Form**:
  ```rust
  // Pure Rust: No FFI needed
  pub struct Timer { ... }
  impl Timer {
      pub fn new() -> Self { ... }
  }
  ```
- **Challenges**: Callback function pointers from C (temporary)

#### 1.2 Config Module  
- **Priority**: HIGH - Used by many components
- **Dependencies**: JSON parsing
- **Rust Benefits**: serde for type-safe serialization
- **Approach**:
  - Use `serde_json` for JSON handling
  - Expose C-compatible structs
  - Handle file I/O with proper error handling
- **Challenges**: String passing between C and Rust

#### 1.3 Audio Module
- **Priority**: MEDIUM - Self-contained
- **Dependencies**: ALSA/pthread
- **Rust Benefits**: Safe concurrency, better audio crates
- **Approach**:
  - Start with ALSA bindings
  - Consider `rodio` or `cpal` crates later
  - Thread safety with Rust's Send/Sync
- **Challenges**: Real-time audio constraints

### Phase 2: System Integration (Weeks 3-4)

#### 2.1 Input Monitor
- **Priority**: MEDIUM - Standalone service
- **Dependencies**: X11 (XScreenSaver extension)
- **Rust Benefits**: Safe X11 bindings with `x11rb`
- **Approach**:
  - Use `x11rb` crate for X11 interaction
  - Implement activity detection with proper error handling
- **Challenges**: X11 event loop integration

#### 2.2 D-Bus Service
- **Priority**: LOW - Optional feature
- **Dependencies**: D-Bus system
- **Rust Benefits**: Type-safe D-Bus with `zbus`
- **Approach**:
  - Use `zbus` for D-Bus implementation
  - Generate interface from XML
  - Async support with tokio
- **Challenges**: GTK main loop integration

### Phase 3: UI Components (Weeks 5-6)

#### 3.1 Tray Icon (Cairo drawing)
- **Priority**: MEDIUM - Visual component
- **Dependencies**: Cairo
- **Rust Benefits**: Safe Cairo bindings
- **Approach**:
  - Use `cairo-rs` crate
  - Port drawing logic preserving visual design
- **Challenges**: Pixel-perfect reproduction

#### 3.2 Break Overlay
- **Priority**: LOW - Standalone window
- **Dependencies**: GTK3
- **Rust Benefits**: gtk-rs bindings
- **Approach**:
  - Use `gtk-rs` for GTK3 bindings
  - Implement as separate window component
- **Challenges**: Multi-monitor handling

### Phase 4: Main Application (Weeks 7-8)

#### 4.1 Settings Dialog
- **Priority**: LOW - Complex UI
- **Dependencies**: GTK3, Config
- **Approach**:
  - Port after Config module is in Rust
  - Use `gtk-rs` with builder pattern
- **Challenges**: Complex widget interactions

#### 4.2 Main Window & App
- **Priority**: FINAL - Coordinates everything
- **Dependencies**: All other modules
- **Approach**:
  - Port last when all dependencies are Rust
  - Full `gtk-rs` application
- **Challenges**: Complete rewrite of main logic

## Technical Implementation Details

### Build System Integration

1. **Cargo Workspace**:
   ```toml
   [workspace]
   members = [
     "rust-timer",
     "rust-config", 
     "rust-audio",
     # ... other modules
   ]
   ```

2. **Modified Makefile**:
   ```makefile
   # Build Rust modules first
   rust-libs:
       cargo build --release
       
   # Link Rust libraries
   RUST_LIBS = -L./target/release -ltimer -lconfig
   ```

3. **C Header Generation**:
   ```toml
   # In each Rust crate
   [build-dependencies]
   cbindgen = "0.24"
   ```

### FFI Patterns

1. **Opaque Pointers**:
   ```rust
   #[repr(C)]
   pub struct Timer {
       _private: [u8; 0],
   }
   ```

2. **Callback Handling**:
   ```rust
   type TimerCallback = extern "C" fn(*mut c_void);
   
   #[no_mangle]
   pub extern "C" fn timer_set_callback(
       timer: *mut Timer,
       cb: TimerCallback,
       user_data: *mut c_void
   ) { ... }
   ```

3. **Error Handling**:
   ```rust
   #[repr(C)]
   pub enum ErrorCode {
       Success = 0,
       InvalidInput = 1,
       IoError = 2,
   }
   ```

### Testing Strategy

1. **Unit Tests**: Test Rust modules independently
2. **Integration Tests**: Test C-Rust FFI boundaries
3. **Regression Tests**: Ensure functionality remains intact
4. **Valgrind**: Check for memory leaks at FFI boundaries

## Risk Mitigation

1. **Rollback Plan**: Keep C modules until Rust equivalents are stable
2. **Feature Flags**: Toggle between C and Rust implementations
3. **Gradual Deployment**: Test each phase thoroughly before proceeding
4. **Performance Monitoring**: Ensure no regression in performance

## Success Metrics

1. **Functional Parity**: All features work identically
2. **Memory Safety**: No segfaults or memory leaks
3. **Performance**: Equal or better performance
4. **Code Quality**: Reduced LOC, better error handling
5. **Maintainability**: Easier to add new features

## Timeline

- **Week 1-2**: Timer, Config, Audio modules
- **Week 3-4**: Input Monitor, D-Bus
- **Week 5-6**: Tray Icon, Break Overlay  
- **Week 7-8**: Settings Dialog, Main App
- **Week 9-10**: Testing, optimization, documentation

## Next Steps

1. Set up Rust workspace structure
2. Create first FFI module (Timer recommended)
3. Integrate with existing Makefile
4. Test C-Rust interop thoroughly
5. Proceed with remaining modules

## Final Architecture Vision

### Pure Rust Application Structure

Once migration is complete, the ideal end state is:

```
commodoro (Pure Rust Binary)
├── main.rs                 # Application entry point
├── timer/                  # Timer state machine (no FFI)
├── config/                 # Serde-based configuration
├── audio/                  # Rodio/CPAL for audio
├── gui/
│   ├── app.rs             # GTK-rs application
│   ├── main_window.rs     # GTK-rs main window
│   ├── tray.rs            # System tray with gtk-rs
│   ├── break_overlay.rs   # GTK-rs overlay
│   └── settings.rs        # GTK-rs settings dialog
├── input/                  # x11rb for input monitoring
└── dbus/                   # zbus for D-Bus service
```

### When to Keep C Modules via FFI

We may need to maintain some C modules if:

1. **Performance Critical**: C implementation significantly outperforms Rust alternatives
2. **Missing Rust Bindings**: No mature Rust crate exists for required functionality
3. **Complex Legacy Code**: Porting cost exceeds maintenance benefit
4. **Platform Specific**: OS-specific code that works better in C

### Decision Criteria for FFI Retention

Keep a C module via FFI only if ALL of these are true:
- No suitable Rust alternative exists
- Creating Rust bindings would be more complex than maintaining C code
- The module is stable and rarely needs changes
- Performance or functionality would suffer in Rust

Examples of potential FFI candidates:
- Low-level X11 extensions without Rust bindings
- Platform-specific audio backends
- Deprecated APIs (like GtkStatusIcon) until alternatives exist

## Conclusion

This plan treats FFI as a transitional mechanism, not a permanent architecture. The goal is a pure Rust application leveraging the Rust ecosystem (gtk-rs, serde, tokio, etc.) with C modules retained via FFI only where absolutely necessary. This approach provides the benefits of incremental migration while keeping the end goal of a modern, safe, and maintainable Rust codebase.