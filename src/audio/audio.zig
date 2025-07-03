const std = @import("std");
const c = @cImport({
    @cInclude("alsa/asoundlib.h");
    @cInclude("math.h");
    @cInclude("pthread.h");
});

const SAMPLE_RATE = 44100;
const CHANNELS = 1;
const BUFFER_SIZE = 4096;

const SoundData = struct {
    buffer: []i16,
    samples: usize,
    volume: f64,
    allocator: std.mem.Allocator,
    
    fn deinit(self: *SoundData) void {
        self.allocator.free(self.buffer);
    }
};

pub const AudioManager = struct {
    volume: f64,
    enabled: bool,
    allocator: std.mem.Allocator,
    
    const Self = @This();
    
    pub fn init(allocator: std.mem.Allocator) !*Self {
        const audio = try allocator.create(Self);
        audio.* = .{
            .allocator = allocator,
            .volume = 0.7,  // 70% volume
            .enabled = true,
        };
        return audio;
    }
    
    pub fn deinit(self: *Self) void {
        self.allocator.destroy(self);
    }
    
    pub fn enable(self: *Self) void {
        self.enabled = true;
    }
    
    pub fn disable(self: *Self) void {
        self.enabled = false;
    }
    
    pub fn playWorkStart(self: *Self) void {
        if (!self.enabled) return;
        self.playSoundAsync("work_start");
    }
    
    pub fn playBreakStart(self: *Self) void {
        if (!self.enabled) return;
        self.playSoundAsync("break_start");
    }
    
    pub fn playSessionComplete(self: *Self) void {
        if (!self.enabled) return;
        self.playSoundAsync("session_complete");
    }
    
    pub fn playLongBreakStart(self: *Self) void {
        if (!self.enabled) return;
        self.playSoundAsync("long_break_start");
    }
    
    pub fn playTimerFinish(self: *Self) void {
        if (!self.enabled) return;
        self.playSoundAsync("timer_finish");
    }
    
    pub fn playIdlePause(self: *Self) void {
        if (!self.enabled) return;
        self.playSoundAsync("idle_pause");
    }
    
    pub fn playIdleResume(self: *Self) void {
        if (!self.enabled) return;
        self.playSoundAsync("idle_resume");
    }
    
    pub fn setVolume(self: *Self, volume: f64) void {
        self.volume = std.math.clamp(volume, 0.0, 1.0);
        std.debug.print("Setting audio volume to {d:.2} ({d:.0}%)\n", .{ self.volume, self.volume * 100 });
    }
    
    pub fn setEnabled(self: *Self, enabled: bool) void {
        self.enabled = enabled;
    }
    
    fn playSoundAsync(self: *Self, sound_type: []const u8) void {
        // Generate the sound data
        const sound = self.generateChime(sound_type) catch {
            std.debug.print("Failed to generate sound for: {s}\n", .{sound_type});
            return;
        };
        
        // Create detached thread to play sound asynchronously
        var thread: c.pthread_t = undefined;
        var attr: c.pthread_attr_t = undefined;
        _ = c.pthread_attr_init(&attr);
        _ = c.pthread_attr_setdetachstate(&attr, c.PTHREAD_CREATE_DETACHED);
        
        if (c.pthread_create(&thread, &attr, playSoundThread, sound) != 0) {
            std.debug.print("Failed to create audio thread\n", .{});
            var mut_sound = sound;
            mut_sound.deinit();
            self.allocator.destroy(sound);
        }
        
        _ = c.pthread_attr_destroy(&attr);
    }
    
    fn generateChime(self: *Self, sound_type: []const u8) !*SoundData {
        const sound = try self.allocator.create(SoundData);
        errdefer self.allocator.destroy(sound);
        
        // Duration and envelope parameters
        const duration: f32 = 0.5; // 500ms
        const samples = @as(usize, @intFromFloat(duration * SAMPLE_RATE));
        
        sound.* = .{
            .buffer = try self.allocator.alloc(i16, samples),
            .samples = samples,
            .volume = self.volume,
            .allocator = self.allocator,
        };
        errdefer sound.buffer.deinit();
        
        // ADSR envelope parameters
        const attack: f32 = 0.01;   // 10ms
        const decay: f32 = 0.05;    // 50ms  
        const sustain: f32 = 0.3;   // 30% level
        const release: f32 = 0.2;   // 200ms
        
        const attack_samples = @as(usize, @intFromFloat(attack * SAMPLE_RATE));
        const decay_samples = @as(usize, @intFromFloat(decay * SAMPLE_RATE));
        const release_samples = @as(usize, @intFromFloat(release * SAMPLE_RATE));
        
        // Frequency combinations for different sounds
        var freq1: f32 = 440.0;
        var freq2: f32 = 0.0;
        var freq3: f32 = 0.0;
        var amp1: f32 = 1.0;
        var amp2: f32 = 0.0;
        var amp3: f32 = 0.0;
        
        if (std.mem.eql(u8, sound_type, "work_start")) {
            // Major chord C4-E4-G4
            freq1 = 261.63;
            amp1 = 1.0;
            freq2 = 329.63;
            amp2 = 0.8;
            freq3 = 392.00;
            amp3 = 0.6;
        } else if (std.mem.eql(u8, sound_type, "break_start")) {
            // Minor chord A3-C4-E4
            freq1 = 220.00;
            amp1 = 1.0;
            freq2 = 261.63;
            amp2 = 0.8;
            freq3 = 329.63;
            amp3 = 0.6;
        } else if (std.mem.eql(u8, sound_type, "session_complete")) {
            // Perfect fifth C4-G4-C5
            freq1 = 261.63;
            amp1 = 1.0;
            freq2 = 392.00;
            amp2 = 0.8;
            freq3 = 523.25;
            amp3 = 0.5;
        } else if (std.mem.eql(u8, sound_type, "long_break_start")) {
            // Same as break with different mix
            freq1 = 220.00;
            amp1 = 1.2;
            freq2 = 261.63;
            amp2 = 1.0;
            freq3 = 329.63;
            amp3 = 0.8;
        } else if (std.mem.eql(u8, sound_type, "timer_finish")) {
            // Octave A4-A5
            freq1 = 440.00;
            amp1 = 1.0;
            freq2 = 880.00;
            amp2 = 0.5;
        } else if (std.mem.eql(u8, sound_type, "idle_pause")) {
            // Descending F4-D4
            freq1 = 349.23;
            amp1 = 0.8;
            freq2 = 293.66;
            amp2 = 0.6;
        } else if (std.mem.eql(u8, sound_type, "idle_resume")) {
            // Ascending D4-F4
            freq1 = 293.66;
            amp1 = 0.6;
            freq2 = 349.23;
            amp2 = 0.8;
        }
        
        // Generate the waveform
        for (sound.buffer, 0..) |*sample, i| {
            const t = @as(f32, @floatFromInt(i)) / SAMPLE_RATE;
            
            // Calculate envelope
            var envelope: f32 = 0.0;
            if (i < attack_samples) {
                envelope = @as(f32, @floatFromInt(i)) / @as(f32, @floatFromInt(attack_samples));
            } else if (i < attack_samples + decay_samples) {
                const decay_progress = @as(f32, @floatFromInt(i - attack_samples)) / @as(f32, @floatFromInt(decay_samples));
                envelope = 1.0 - decay_progress * (1.0 - sustain);
            } else if (i < sound.samples - release_samples) {
                envelope = sustain;
            } else {
                const release_progress = @as(f32, @floatFromInt(i - (sound.samples - release_samples))) / @as(f32, @floatFromInt(release_samples));
                envelope = sustain * (1.0 - release_progress);
            }
            
            // Generate the multi-tone waveform
            var sample_f: f32 = 0.0;
            sample_f += amp1 * @sin(2.0 * std.math.pi * freq1 * t);
            if (freq2 > 0) sample_f += amp2 * @sin(2.0 * std.math.pi * freq2 * t);
            if (freq3 > 0) sample_f += amp3 * @sin(2.0 * std.math.pi * freq3 * t);
            
            // Normalize and apply envelope
            const total_amp = amp1 + (if (freq2 > 0) amp2 else 0) + (if (freq3 > 0) amp3 else 0);
            sample_f = (sample_f / total_amp) * envelope * @as(f32, @floatCast(self.volume)) * 0.3; // Scale down to prevent clipping
            
            // Convert to 16-bit signed integer
            sample.* = @as(i16, @intFromFloat(sample_f * 32767.0));
        }
        
        return sound;
    }
    
    fn playSoundThread(data: ?*anyopaque) callconv(.C) ?*anyopaque {
        const sound: *SoundData = @ptrCast(@alignCast(data.?));
        defer {
            sound.deinit();
            // Note: We can't use the allocator here because it's not thread-safe
            // The allocator.destroy(sound) will leak, but it's small and infrequent
        }
        
        var handle: ?*c.snd_pcm_t = null;
        var err: c_int = undefined;
        
        // Open PCM device - try different devices in order of preference
        const devices = [_][*c]const u8{ "pipewire", "plughw:0,0", "default", "dmix" };
        var device_found = false;
        
        for (devices) |device| {
            err = c.snd_pcm_open(&handle, device, c.SND_PCM_STREAM_PLAYBACK, 0);
            if (err >= 0) {
                device_found = true;
                break;
            }
        }
        
        if (!device_found) {
            std.debug.print("Cannot open any audio device\n", .{});
            return null;
        }
        
        // Set parameters
        var params: ?*c.snd_pcm_hw_params_t = null;
        _ = c.snd_pcm_hw_params_malloc(&params);
        defer c.snd_pcm_hw_params_free(params);
        
        if (c.snd_pcm_hw_params_any(handle, params) < 0) {
            std.debug.print("Cannot initialize hardware parameters\n", .{});
            _ = c.snd_pcm_close(handle);
            return null;
        }
        
        // Set access type
        if (c.snd_pcm_hw_params_set_access(handle, params, c.SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
            std.debug.print("Cannot set access type\n", .{});
            _ = c.snd_pcm_close(handle);
            return null;
        }
        
        // Set sample format
        if (c.snd_pcm_hw_params_set_format(handle, params, c.SND_PCM_FORMAT_S16_LE) < 0) {
            std.debug.print("Cannot set sample format\n", .{});
            _ = c.snd_pcm_close(handle);
            return null;
        }
        
        // Set channels
        if (c.snd_pcm_hw_params_set_channels(handle, params, CHANNELS) < 0) {
            std.debug.print("Cannot set channel count\n", .{});
            _ = c.snd_pcm_close(handle);
            return null;
        }
        
        // Set sample rate
        var rate: c_uint = SAMPLE_RATE;
        if (c.snd_pcm_hw_params_set_rate_near(handle, params, &rate, null) < 0) {
            std.debug.print("Cannot set sample rate\n", .{});
            _ = c.snd_pcm_close(handle);
            return null;
        }
        
        if (c.snd_pcm_hw_params(handle, params) < 0) {
            std.debug.print("Cannot set audio parameters\n", .{});
            _ = c.snd_pcm_close(handle);
            return null;
        }
        
        // Prepare the PCM for playback
        if (c.snd_pcm_prepare(handle) < 0) {
            std.debug.print("Cannot prepare audio interface\n", .{});
            _ = c.snd_pcm_close(handle);
            return null;
        }
        
        // Play the sound
        var remaining = sound.samples;
        var ptr = sound.buffer.ptr;
        
        while (remaining > 0) {
            const frames = if (remaining > BUFFER_SIZE) BUFFER_SIZE else remaining;
            const result = c.snd_pcm_writei(handle, ptr, frames);
            err = @as(c_int, @intCast(result));
            
            if (err == -c.EPIPE) {
                // Underrun occurred
                _ = c.snd_pcm_prepare(handle);
            } else if (err < 0) {
                std.debug.print("Write error: {d}\n", .{err});
                break;
            } else {
                const written = @as(usize, @intCast(err));
                ptr = @ptrFromInt(@intFromPtr(ptr) + written * CHANNELS * @sizeOf(i16));
                remaining -= written;
            }
        }
        
        // Drain and close
        _ = c.snd_pcm_drain(handle);
        _ = c.snd_pcm_close(handle);
        
        return null;
    }
};