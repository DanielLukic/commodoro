use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;
use std::f32::consts::PI;
use libc::c_int;
use alsa::{Direction, ValueOr};
use alsa::pcm::{PCM, HwParams, Format, Access};

const SAMPLE_RATE: u32 = 44100;
const CHANNELS: u32 = 1;
const BUFFER_SIZE: usize = 4096;

#[repr(C)]
pub struct AudioManager {
    volume: f64,
    enabled: AtomicBool,
}

struct SoundData {
    buffer: Vec<i16>,
}

impl AudioManager {
    fn new() -> Self {
        AudioManager {
            volume: 0.7,
            enabled: AtomicBool::new(true),
        }
    }

    fn is_enabled(&self) -> bool {
        self.enabled.load(Ordering::Relaxed)
    }

    fn play_sound(&self, sound_type: &str) {
        if !self.is_enabled() {
            return;
        }

        let sound = match generate_chime(sound_type, self.volume as f32) {
            Some(s) => s,
            None => {
                eprintln!("Failed to generate sound for: {}", sound_type);
                return;
            }
        };

        // Play sound in a separate thread
        thread::spawn(move || {
            if let Err(e) = play_sound_alsa(&sound) {
                eprintln!("Audio playback error: {}", e);
            }
        });
    }
}

fn play_sound_alsa(sound: &SoundData) -> Result<(), alsa::Error> {
    // Open PCM device
    let pcm = PCM::new("default", Direction::Playback, false)?;
    
    // Configure hardware parameters
    {
        let hwp = HwParams::any(&pcm)?;
        hwp.set_format(Format::s16())?;
        hwp.set_access(Access::RWInterleaved)?;
        hwp.set_channels(CHANNELS)?;
        hwp.set_rate(SAMPLE_RATE, ValueOr::Nearest)?;
        hwp.set_buffer_size_near((sound.buffer.len() * 2) as i64)?;
        pcm.hw_params(&hwp)?;
    }

    // Write sound data
    let io = pcm.io_i16()?;
    let mut offset = 0;
    while offset < sound.buffer.len() {
        let remaining = sound.buffer.len() - offset;
        let to_write = remaining.min(BUFFER_SIZE);
        
        match io.writei(&sound.buffer[offset..offset + to_write]) {
            Ok(frames) => offset += frames,
            Err(e) => {
                // Check for EPIPE (broken pipe / buffer underrun)
                // EPIPE is -32 in ALSA
                if e.errno() == libc::EPIPE {
                    // Buffer underrun, recover
                    pcm.prepare()?;
                } else {
                    return Err(e);
                }
            }
        }
    }
    
    // Drain buffer to ensure all samples are played
    pcm.drain()?;
    
    Ok(())
}

fn generate_chime(sound_type: &str, volume: f32) -> Option<SoundData> {
    // Duration and envelope parameters
    let duration = 0.5; // 500ms
    let samples = (duration * SAMPLE_RATE as f64) as usize;
    let mut buffer = vec![0i16; samples];
    
    // ADSR envelope parameters
    let attack = 0.01;   // 10ms
    let decay = 0.05;    // 50ms
    let sustain = 0.3;   // 30% level
    let release = 0.2;   // 200ms
    
    let attack_samples = (attack * SAMPLE_RATE as f64) as usize;
    let decay_samples = (decay * SAMPLE_RATE as f64) as usize;
    let release_samples = (release * SAMPLE_RATE as f64) as usize;
    
    // Frequency combinations for different sounds
    let (freq1, amp1, freq2, amp2, freq3, amp3) = match sound_type {
        "work_start" => {
            // Major chord C4-E4-G4
            (261.63, 1.0, 329.63, 0.8, 392.00, 0.6)
        }
        "break_start" => {
            // Minor chord A3-C4-E4
            (220.00, 1.0, 261.63, 0.8, 329.63, 0.6)
        }
        "session_complete" => {
            // Perfect fifth C4-G4-C5
            (261.63, 1.0, 392.00, 0.8, 523.25, 0.5)
        }
        "long_break_start" => {
            // Same as break with different mix
            (220.00, 1.2, 261.63, 1.0, 329.63, 0.8)
        }
        "timer_finish" => {
            // Octave A4-A5
            (440.00, 1.0, 880.00, 0.5, 0.0, 0.0)
        }
        "idle_pause" => {
            // Descending F4-D4
            (349.23, 0.8, 293.66, 0.6, 0.0, 0.0)
        }
        "idle_resume" => {
            // Ascending D4-F4
            (293.66, 0.6, 349.23, 0.8, 0.0, 0.0)
        }
        _ => {
            // Default tone
            (440.0, 1.0, 0.0, 0.0, 0.0, 0.0)
        }
    };
    
    // Generate the waveform
    for i in 0..samples {
        let t = i as f32 / SAMPLE_RATE as f32;
        
        // Calculate envelope
        let envelope = if i < attack_samples {
            i as f32 / attack_samples as f32
        } else if i < attack_samples + decay_samples {
            let decay_progress = (i - attack_samples) as f32 / decay_samples as f32;
            1.0 - decay_progress * (1.0 - sustain as f32)
        } else if i < samples - release_samples {
            sustain as f32
        } else {
            let release_progress = (i - (samples - release_samples)) as f32 / release_samples as f32;
            sustain as f32 * (1.0 - release_progress)
        };
        
        // Generate the multi-tone waveform
        let mut sample = 0.0;
        sample += amp1 as f32 * (2.0 * PI * freq1 as f32 * t).sin();
        if freq2 > 0.0 {
            sample += amp2 as f32 * (2.0 * PI * freq2 as f32 * t).sin();
        }
        if freq3 > 0.0 {
            sample += amp3 as f32 * (2.0 * PI * freq3 as f32 * t).sin();
        }
        
        // Normalize and apply envelope
        let total_amp = amp1 as f32 + if freq2 > 0.0 { amp2 as f32 } else { 0.0 } + if freq3 > 0.0 { amp3 as f32 } else { 0.0 };
        sample = (sample / total_amp) * envelope * volume * 0.3; // Scale down to prevent clipping
        
        // Convert to 16-bit signed integer
        buffer[i] = (sample * 32767.0) as i16;
    }
    
    Some(SoundData { buffer })
}

// C FFI exports
#[no_mangle]
pub extern "C" fn audio_manager_new() -> *mut AudioManager {
    Box::into_raw(Box::new(AudioManager::new()))
}

#[no_mangle]
pub extern "C" fn audio_manager_free(audio: *mut AudioManager) {
    if audio.is_null() {
        return;
    }
    unsafe {
        drop(Box::from_raw(audio));
    }
}

#[no_mangle]
pub extern "C" fn audio_manager_play_work_start(audio: *mut AudioManager) {
    if audio.is_null() {
        return;
    }
    unsafe {
        (*audio).play_sound("work_start");
    }
}

#[no_mangle]
pub extern "C" fn audio_manager_play_break_start(audio: *mut AudioManager) {
    if audio.is_null() {
        return;
    }
    unsafe {
        (*audio).play_sound("break_start");
    }
}

#[no_mangle]
pub extern "C" fn audio_manager_play_session_complete(audio: *mut AudioManager) {
    if audio.is_null() {
        return;
    }
    unsafe {
        (*audio).play_sound("session_complete");
    }
}

#[no_mangle]
pub extern "C" fn audio_manager_play_long_break_start(audio: *mut AudioManager) {
    if audio.is_null() {
        return;
    }
    unsafe {
        (*audio).play_sound("long_break_start");
    }
}

#[no_mangle]
pub extern "C" fn audio_manager_play_timer_finish(audio: *mut AudioManager) {
    if audio.is_null() {
        return;
    }
    unsafe {
        (*audio).play_sound("timer_finish");
    }
}

#[no_mangle]
pub extern "C" fn audio_manager_play_idle_pause(audio: *mut AudioManager) {
    if audio.is_null() {
        return;
    }
    unsafe {
        (*audio).play_sound("idle_pause");
    }
}

#[no_mangle]
pub extern "C" fn audio_manager_play_idle_resume(audio: *mut AudioManager) {
    if audio.is_null() {
        return;
    }
    unsafe {
        (*audio).play_sound("idle_resume");
    }
}

#[no_mangle]
pub extern "C" fn audio_manager_set_volume(audio: *mut AudioManager, volume: f64) {
    if audio.is_null() {
        return;
    }
    
    // Clamp volume to valid range
    let volume = volume.clamp(0.0, 1.0);
    
    println!("Setting audio volume to {:.2} ({:.0}%)", volume, volume * 100.0);
    
    unsafe {
        (*audio).volume = volume;
    }
}

#[no_mangle]
pub extern "C" fn audio_manager_set_enabled(audio: *mut AudioManager, enabled: c_int) {
    if audio.is_null() {
        return;
    }
    unsafe {
        (*audio).enabled.store(enabled != 0, Ordering::Relaxed);
    }
}