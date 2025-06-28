#define _GNU_SOURCE
#include "audio.h"
#include <glib.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <pthread.h>

#define SAMPLE_RATE 44100
#define CHANNELS 1
#define BUFFER_SIZE 4096

typedef struct {
    short *buffer;
    int samples;
    double volume;
} SoundData;

struct _AudioManager {
    double volume;
    gboolean enabled;
};

static void* play_sound_thread(void *data);
static void play_sound_async(AudioManager *audio, const char *sound_type);
static SoundData* generate_chime(const char *sound_type, double volume);
static void free_sound_data(SoundData *data);

AudioManager* audio_manager_new(void) {
    AudioManager *audio = g_malloc0(sizeof(AudioManager));
    
    // Set default values
    audio->volume = 0.7;  // 70% volume
    audio->enabled = TRUE;
    
    return audio;
}

void audio_manager_free(AudioManager *audio) {
    if (!audio) return;
    g_free(audio);
}

void audio_manager_play_work_start(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_async(audio, "work_start");
}

void audio_manager_play_break_start(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_async(audio, "break_start");
}

void audio_manager_play_session_complete(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_async(audio, "session_complete");
}

void audio_manager_play_long_break_start(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_async(audio, "long_break_start");
}

void audio_manager_play_timer_finish(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_async(audio, "timer_finish");
}

void audio_manager_play_idle_pause(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_async(audio, "idle_pause");
}

void audio_manager_play_idle_resume(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_async(audio, "idle_resume");
}

void audio_manager_set_volume(AudioManager *audio, double volume) {
    if (!audio) return;
    
    // Clamp volume to valid range
    if (volume < 0.0) volume = 0.0;
    if (volume > 1.0) volume = 1.0;
    
    g_print("Setting audio volume to %.2f (%.0f%%)\n", volume, volume * 100);
    
    audio->volume = volume;
}

void audio_manager_set_enabled(AudioManager *audio, gboolean enabled) {
    if (!audio) return;
    audio->enabled = enabled;
}

static void* play_sound_thread(void *data) {
    SoundData *sound = (SoundData*)data;
    
    snd_pcm_t *handle;
    int err;
    
    // Open PCM device - try different devices in order of preference
    const char* devices[] = {"pipewire", "plughw:0,0", "default", "dmix"};
    int device_found = 0;
    
    for (int i = 0; i < 4; i++) {
        if ((err = snd_pcm_open(&handle, devices[i], SND_PCM_STREAM_PLAYBACK, 0)) >= 0) {
            device_found = 1;
            break;
        }
    }
    
    if (!device_found) {
        g_warning("Cannot open any audio device");
        free_sound_data(sound);
        return NULL;
    }
    
    // Set parameters
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    
    if ((err = snd_pcm_hw_params_any(handle, params)) < 0) {
        g_warning("Cannot initialize hardware parameters: %s", snd_strerror(err));
        snd_pcm_close(handle);
        free_sound_data(sound);
        return NULL;
    }
    
    // Set access type
    if ((err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        g_warning("Cannot set access type: %s", snd_strerror(err));
        snd_pcm_close(handle);
        free_sound_data(sound);
        return NULL;
    }
    
    // Set sample format to S16_LE which is more widely supported
    if ((err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE)) < 0) {
        g_warning("Cannot set sample format: %s", snd_strerror(err));
        snd_pcm_close(handle);
        free_sound_data(sound);
        return NULL;
    }
    
    // Set channels
    if ((err = snd_pcm_hw_params_set_channels(handle, params, CHANNELS)) < 0) {
        g_warning("Cannot set channel count: %s", snd_strerror(err));
        snd_pcm_close(handle);
        free_sound_data(sound);
        return NULL;
    }
    
    // Set sample rate
    unsigned int rate = SAMPLE_RATE;
    if ((err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0)) < 0) {
        g_warning("Cannot set sample rate: %s", snd_strerror(err));
        snd_pcm_close(handle);
        free_sound_data(sound);
        return NULL;
    }
    
    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        g_warning("Cannot set audio parameters: %s", snd_strerror(err));
        snd_pcm_close(handle);
        free_sound_data(sound);
        return NULL;
    }
    
    // Prepare the PCM for playback
    if ((err = snd_pcm_prepare(handle)) < 0) {
        g_warning("Cannot prepare audio interface: %s", snd_strerror(err));
        snd_pcm_close(handle);
        free_sound_data(sound);
        return NULL;
    }
    
    // Play the sound
    int remaining = sound->samples;
    short *ptr = sound->buffer;
    
    while (remaining > 0) {
        int frames = remaining > BUFFER_SIZE ? BUFFER_SIZE : remaining;
        err = snd_pcm_writei(handle, ptr, frames);
        
        if (err == -EPIPE) {
            // Underrun occurred
            snd_pcm_prepare(handle);
        } else if (err < 0) {
            g_warning("Write error: %s", snd_strerror(err));
            break;
        } else {
            ptr += err * CHANNELS;
            remaining -= err;
        }
    }
    
    // Drain and close
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    
    free_sound_data(sound);
    return NULL;
}

static void play_sound_async(AudioManager *audio, const char *sound_type) {
    if (!audio) return;
    
    // Generate the sound data
    SoundData *sound = generate_chime(sound_type, audio->volume);
    if (!sound) {
        g_warning("Failed to generate sound for: %s", sound_type);
        return;
    }
    
    // Create detached thread to play sound asynchronously
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    if (pthread_create(&thread, &attr, play_sound_thread, sound) != 0) {
        g_warning("Failed to create audio thread");
        free_sound_data(sound);
    }
    
    pthread_attr_destroy(&attr);
}

static SoundData* generate_chime(const char *sound_type, double volume) {
    SoundData *sound = g_malloc(sizeof(SoundData));
    sound->volume = volume;
    
    // Duration and envelope parameters
    float duration = 0.5f;  // 500ms
    sound->samples = (int)(duration * SAMPLE_RATE);
    sound->buffer = g_malloc(sound->samples * sizeof(short));
    
    // ADSR envelope parameters
    float attack = 0.01f;   // 10ms
    float decay = 0.05f;    // 50ms  
    float sustain = 0.3f;   // 30% level
    float release = 0.2f;   // 200ms
    
    int attack_samples = (int)(attack * SAMPLE_RATE);
    int decay_samples = (int)(decay * SAMPLE_RATE);
    int release_samples = (int)(release * SAMPLE_RATE);
    
    // Frequency combinations for different sounds
    float freq1 = 440.0f, freq2 = 0.0f, freq3 = 0.0f;
    float amp1 = 1.0f, amp2 = 0.0f, amp3 = 0.0f;
    
    if (g_strcmp0(sound_type, "work_start") == 0) {
        // Major chord C4-E4-G4
        freq1 = 261.63f; amp1 = 1.0f;
        freq2 = 329.63f; amp2 = 0.8f;
        freq3 = 392.00f; amp3 = 0.6f;
    } else if (g_strcmp0(sound_type, "break_start") == 0) {
        // Minor chord A3-C4-E4
        freq1 = 220.00f; amp1 = 1.0f;
        freq2 = 261.63f; amp2 = 0.8f;
        freq3 = 329.63f; amp3 = 0.6f;
    } else if (g_strcmp0(sound_type, "session_complete") == 0) {
        // Perfect fifth C4-G4-C5
        freq1 = 261.63f; amp1 = 1.0f;
        freq2 = 392.00f; amp2 = 0.8f;
        freq3 = 523.25f; amp3 = 0.5f;
    } else if (g_strcmp0(sound_type, "long_break_start") == 0) {
        // Same as break with different mix
        freq1 = 220.00f; amp1 = 1.2f;
        freq2 = 261.63f; amp2 = 1.0f;
        freq3 = 329.63f; amp3 = 0.8f;
    } else if (g_strcmp0(sound_type, "timer_finish") == 0) {
        // Octave A4-A5
        freq1 = 440.00f; amp1 = 1.0f;
        freq2 = 880.00f; amp2 = 0.5f;
    } else if (g_strcmp0(sound_type, "idle_pause") == 0) {
        // Descending F4-D4
        freq1 = 349.23f; amp1 = 0.8f;
        freq2 = 293.66f; amp2 = 0.6f;
    } else if (g_strcmp0(sound_type, "idle_resume") == 0) {
        // Ascending D4-F4
        freq1 = 293.66f; amp1 = 0.6f;
        freq2 = 349.23f; amp2 = 0.8f;
    }
    
    // Generate the waveform
    for (int i = 0; i < sound->samples; i++) {
        float t = (float)i / SAMPLE_RATE;
        
        // Calculate envelope
        float envelope = 0.0f;
        if (i < attack_samples) {
            envelope = (float)i / attack_samples;
        } else if (i < attack_samples + decay_samples) {
            float decay_progress = (float)(i - attack_samples) / decay_samples;
            envelope = 1.0f - decay_progress * (1.0f - sustain);
        } else if (i < sound->samples - release_samples) {
            envelope = sustain;
        } else {
            float release_progress = (float)(i - (sound->samples - release_samples)) / release_samples;
            envelope = sustain * (1.0f - release_progress);
        }
        
        // Generate the multi-tone waveform
        float sample = 0.0f;
        sample += amp1 * sinf(2.0f * M_PI * freq1 * t);
        if (freq2 > 0) sample += amp2 * sinf(2.0f * M_PI * freq2 * t);
        if (freq3 > 0) sample += amp3 * sinf(2.0f * M_PI * freq3 * t);
        
        // Normalize and apply envelope
        float total_amp = amp1 + (freq2 > 0 ? amp2 : 0) + (freq3 > 0 ? amp3 : 0);
        sample = (sample / total_amp) * envelope * volume * 0.3f;  // Scale down to prevent clipping
        
        // Convert to 16-bit signed integer
        sound->buffer[i] = (short)(sample * 32767.0f);
    }
    
    return sound;
}

static void free_sound_data(SoundData *data) {
    if (data) {
        g_free(data->buffer);
        g_free(data);
    }
}