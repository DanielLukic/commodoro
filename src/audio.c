#include "audio.h"
#include <gst/gst.h>
#include <glib.h>

struct _AudioManager {
    GstElement *playbin;
    double volume;
    gboolean enabled;
    gboolean gst_initialized;
};

static gboolean on_bus_message(GstBus *bus, GstMessage *message, AudioManager *audio);
static void play_sound_file(AudioManager *audio, const char *sound_type);
static gboolean stop_pipeline_timeout(gpointer user_data);

AudioManager* audio_manager_new(void) {
    AudioManager *audio = g_malloc0(sizeof(AudioManager));
    
    // Initialize GStreamer
    GError *error = NULL;
    if (!gst_init_check(NULL, NULL, &error)) {
        g_warning("Failed to initialize GStreamer: %s", error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        audio->gst_initialized = FALSE;
    } else {
        audio->gst_initialized = TRUE;
    }
    
    // Create playbin element
    if (audio->gst_initialized) {
        audio->playbin = gst_element_factory_make("playbin", "audio-player");
        if (!audio->playbin) {
            g_warning("Failed to create GStreamer playbin element");
            audio->gst_initialized = FALSE;
        } else {
            // Set up bus for handling messages
            GstBus *bus = gst_element_get_bus(audio->playbin);
            gst_bus_add_watch(bus, (GstBusFunc)on_bus_message, audio);
            gst_object_unref(bus);
        }
    }
    
    // Set default values
    audio->volume = 0.7;  // 70% volume
    audio->enabled = TRUE;
    
    // Set initial volume
    if (audio->playbin) {
        g_object_set(audio->playbin, "volume", audio->volume, NULL);
    }
    
    return audio;
}

void audio_manager_free(AudioManager *audio) {
    if (!audio) return;
    
    if (audio->playbin) {
        gst_element_set_state(audio->playbin, GST_STATE_NULL);
        gst_object_unref(audio->playbin);
    }
    
    g_free(audio);
}

void audio_manager_play_work_start(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_file(audio, "work_start");
}

void audio_manager_play_break_start(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_file(audio, "break_start");
}

void audio_manager_play_session_complete(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_file(audio, "session_complete");
}

void audio_manager_play_long_break_start(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_file(audio, "long_break_start");
}

void audio_manager_play_timer_finish(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_file(audio, "timer_finish");
}

void audio_manager_play_idle_pause(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_file(audio, "idle_pause");
}

void audio_manager_play_idle_resume(AudioManager *audio) {
    if (!audio || !audio->enabled) return;
    play_sound_file(audio, "idle_resume");
}

void audio_manager_set_volume(AudioManager *audio, double volume) {
    if (!audio) return;
    
    // Clamp volume to valid range
    if (volume < 0.0) volume = 0.0;
    if (volume > 1.0) volume = 1.0;
    
    g_print("Setting audio volume to %.2f (%.0f%%)\n", volume, volume * 100);
    
    audio->volume = volume;
    
    if (audio->playbin) {
        g_object_set(audio->playbin, "volume", volume, NULL);
    }
}

void audio_manager_set_enabled(AudioManager *audio, gboolean enabled) {
    if (!audio) return;
    audio->enabled = enabled;
}

static gboolean on_bus_message(GstBus *bus, GstMessage *message, AudioManager *audio) {
    (void)bus;    // Suppress unused parameter warning
    (void)audio;  // Suppress unused parameter warning
    
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error;
            gchar *debug;
            gst_message_parse_error(message, &error, &debug);
            g_warning("Audio playback error: %s", error->message);
            g_error_free(error);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            // End of stream - stop playback
            if (audio->playbin) {
                gst_element_set_state(audio->playbin, GST_STATE_NULL);
            }
            break;
        default:
            break;
    }
    
    return TRUE;
}

static void play_sound_file(AudioManager *audio, const char *sound_type) {
    if (!audio || !audio->gst_initialized || !audio->playbin) {
        // Fallback to system beep if GStreamer is not available
        g_print("Audio notification: %s\n", sound_type);
        return;
    }
    
    // Stop any currently playing sound
    gst_element_set_state(audio->playbin, GST_STATE_NULL);
    
    // Generate different tones based on sound type to match Python's chime system
    char pipeline_desc[512];
    
    // Create multi-tone chimes based on Python's chord system
    if (g_strcmp0(sound_type, "work_start") == 0) {
        // Work start - Uplifting major chord (C4, E4, G4: 261.63, 329.63, 392.00)
        g_snprintf(pipeline_desc, sizeof(pipeline_desc),
                   "audiotestsrc wave=sine freq=261.63 volume=0.1 ! audioconvert ! "
                   "audiomixer name=mix ! audioconvert ! autoaudiosink "
                   "audiotestsrc wave=sine freq=329.63 volume=0.1 ! audioconvert ! mix. "
                   "audiotestsrc wave=sine freq=392.00 volume=0.1 ! audioconvert ! mix.");
    } else if (g_strcmp0(sound_type, "break_start") == 0) {
        // Break start - Relaxing minor chord (A3, C4, E4: 220.00, 261.63, 329.63)
        g_snprintf(pipeline_desc, sizeof(pipeline_desc),
                   "audiotestsrc wave=sine freq=220.00 volume=0.1 ! audioconvert ! "
                   "audiomixer name=mix ! audioconvert ! autoaudiosink "
                   "audiotestsrc wave=sine freq=261.63 volume=0.1 ! audioconvert ! mix. "
                   "audiotestsrc wave=sine freq=329.63 volume=0.1 ! audioconvert ! mix.");
    } else if (g_strcmp0(sound_type, "session_complete") == 0) {
        // Session complete - Achievement sound (C4, G4, C5: 261.63, 392.00, 523.25)
        g_snprintf(pipeline_desc, sizeof(pipeline_desc),
                   "audiotestsrc wave=sine freq=261.63 volume=0.1 ! audioconvert ! "
                   "audiomixer name=mix ! audioconvert ! autoaudiosink "
                   "audiotestsrc wave=sine freq=392.00 volume=0.1 ! audioconvert ! mix. "
                   "audiotestsrc wave=sine freq=523.25 volume=0.1 ! audioconvert ! mix.");
    } else if (g_strcmp0(sound_type, "long_break_start") == 0) {
        // Long break - Same as break but slightly different volume mix
        g_snprintf(pipeline_desc, sizeof(pipeline_desc),
                   "audiotestsrc wave=sine freq=220.00 volume=0.12 ! audioconvert ! "
                   "audiomixer name=mix ! audioconvert ! autoaudiosink "
                   "audiotestsrc wave=sine freq=261.63 volume=0.1 ! audioconvert ! mix. "
                   "audiotestsrc wave=sine freq=329.63 volume=0.08 ! audioconvert ! mix.");
    } else if (g_strcmp0(sound_type, "timer_finish") == 0) {
        // Timer finish - Gentle notification (A4, A5: 440.00, 880.00)
        g_snprintf(pipeline_desc, sizeof(pipeline_desc),
                   "audiotestsrc wave=sine freq=440.00 volume=0.15 ! audioconvert ! "
                   "audiomixer name=mix ! audioconvert ! autoaudiosink "
                   "audiotestsrc wave=sine freq=880.00 volume=0.1 ! audioconvert ! mix.");
    } else if (g_strcmp0(sound_type, "idle_pause") == 0) {
        // Idle pause - Soft descending tone (F4, D4: 349.23, 293.66)
        g_snprintf(pipeline_desc, sizeof(pipeline_desc),
                   "audiotestsrc wave=sine freq=349.23 volume=0.08 ! audioconvert ! "
                   "audiomixer name=mix ! audioconvert ! autoaudiosink "
                   "audiotestsrc wave=sine freq=293.66 volume=0.06 ! audioconvert ! mix.");
    } else if (g_strcmp0(sound_type, "idle_resume") == 0) {
        // Idle resume - Soft ascending tone (D4, F4: 293.66, 349.23)
        g_snprintf(pipeline_desc, sizeof(pipeline_desc),
                   "audiotestsrc wave=sine freq=293.66 volume=0.06 ! audioconvert ! "
                   "audiomixer name=mix ! audioconvert ! autoaudiosink "
                   "audiotestsrc wave=sine freq=349.23 volume=0.08 ! audioconvert ! mix.");
    } else {
        // Fallback - simple tone
        g_snprintf(pipeline_desc, sizeof(pipeline_desc),
                   "audiotestsrc wave=sine freq=440 volume=0.2 ! audioconvert ! autoaudiosink");
    }
    
    
    // Create a temporary pipeline for the tone
    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch(pipeline_desc, &error);
    
    if (pipeline) {
        // Play for 0.5 seconds
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        
        // Set a timeout to stop the sound
        g_timeout_add(500, stop_pipeline_timeout, 
                     g_object_ref_sink(pipeline));
    } else {
        g_warning("Failed to create audio pipeline: %s", 
                 error ? error->message : "Unknown error");
        if (error) g_error_free(error);
    }
}

static gboolean stop_pipeline_timeout(gpointer user_data) {
    GstElement *pipeline = GST_ELEMENT(user_data);
    
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        g_object_unref(pipeline);
    }
    
    return FALSE; // Remove timeout
}