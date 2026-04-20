#ifndef _AUDIO_MANAGER_H_
#define _AUDIO_MANAGER_H_

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Sound event identifiers
 */
typedef enum {
    SOUND_ARMED = 0,    /* Short confirmation beep */
    SOUND_RELEASED,     /* Alert tone */
    SOUND_COMPLETE,     /* Success chime */
    SOUND_ABORT,        /* Warning tone */
    SOUND_ERROR,        /* Error buzzer */
    SOUND_TEST_BEEP,    /* Test sound from settings */
    SOUND_MAX,
} sound_event_t;

/**
 * @brief Initialize the audio manager (creates playback task and queue)
 * @return ESP_OK on success
 */
esp_err_t audio_manager_init(void);

/**
 * @brief Queue a sound for non-blocking playback
 * @param event Sound event to play
 * @return ESP_OK if queued successfully
 */
esp_err_t audio_manager_play(sound_event_t event);

/**
 * @brief Enable or disable sound notifications globally
 * @param enabled true to enable sounds
 */
void audio_manager_set_enabled(bool enabled);

/**
 * @brief Check if sounds are enabled
 */
bool audio_manager_is_enabled(void);

#endif
