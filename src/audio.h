#ifndef AUDIO_H
#define AUDIO_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _AudioManager AudioManager;

/**
 * Creates a new audio manager
 * @return New AudioManager instance
 */
AudioManager* audio_manager_new(void);

/**
 * Frees the audio manager
 * @param audio AudioManager instance to free
 */
void audio_manager_free(AudioManager *audio);

/**
 * Plays work session start sound
 * @param audio AudioManager instance
 */
void audio_manager_play_work_start(AudioManager *audio);

/**
 * Plays break start sound
 * @param audio AudioManager instance  
 */
void audio_manager_play_break_start(AudioManager *audio);

/**
 * Plays session complete sound
 * @param audio AudioManager instance
 */
void audio_manager_play_session_complete(AudioManager *audio);

/**
 * Plays long break start sound
 * @param audio AudioManager instance
 */
void audio_manager_play_long_break_start(AudioManager *audio);

/**
 * Plays timer finish sound (when breaks end)
 * @param audio AudioManager instance
 */
void audio_manager_play_timer_finish(AudioManager *audio);

/**
 * Plays idle pause sound (when pausing due to idle)
 * @param audio AudioManager instance
 */
void audio_manager_play_idle_pause(AudioManager *audio);

/**
 * Plays idle resume sound (when resuming from idle)
 * @param audio AudioManager instance
 */
void audio_manager_play_idle_resume(AudioManager *audio);

/**
 * Sets master volume (0.0 to 1.0)
 * @param audio AudioManager instance
 * @param volume Volume level
 */
void audio_manager_set_volume(AudioManager *audio, double volume);

/**
 * Enables or disables audio notifications
 * @param audio AudioManager instance
 * @param enabled TRUE to enable, FALSE to disable
 */
void audio_manager_set_enabled(AudioManager *audio, gboolean enabled);

G_END_DECLS

#endif // AUDIO_H