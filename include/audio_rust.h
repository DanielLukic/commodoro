#ifndef AUDIO_RUST_H
#define AUDIO_RUST_H

#include <glib.h>

G_BEGIN_DECLS

// Forward declaration to match C header
typedef struct AudioManager AudioManager;

// Audio manager functions - matching C API exactly
AudioManager* audio_manager_new(void);
void audio_manager_free(AudioManager *audio);
void audio_manager_play_work_start(AudioManager *audio);
void audio_manager_play_break_start(AudioManager *audio);
void audio_manager_play_session_complete(AudioManager *audio);
void audio_manager_play_long_break_start(AudioManager *audio);
void audio_manager_play_timer_finish(AudioManager *audio);
void audio_manager_play_idle_pause(AudioManager *audio);
void audio_manager_play_idle_resume(AudioManager *audio);
void audio_manager_set_volume(AudioManager *audio, double volume);
void audio_manager_set_enabled(AudioManager *audio, gboolean enabled);

G_END_DECLS

#endif // AUDIO_RUST_H