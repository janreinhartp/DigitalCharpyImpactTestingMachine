#ifndef _BSP_AUDIO_H_
#define _BSP_AUDIO_H_

#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"

#define AUDIO_TAG "AUDIO"
#define AUDIO_INFO(fmt, ...) ESP_LOGI(AUDIO_TAG, fmt, ##__VA_ARGS__)
#define AUDIO_DEBUG(fmt, ...) ESP_LOGD(AUDIO_TAG, fmt, ##__VA_ARGS__)
#define AUDIO_ERROR(fmt, ...) ESP_LOGE(AUDIO_TAG, fmt, ##__VA_ARGS__)

/* I2S audio GPIO pins (CrowPanel built-in speaker) */
#define AUDIO_GPIO_LRCLK   21   /* Left-Right Clock (Word Select) */
#define AUDIO_GPIO_BCLK    22   /* Bit Clock */
#define AUDIO_GPIO_SDATA   23   /* Serial Data Out */
#define AUDIO_GPIO_CTRL    30   /* Amplifier enable (active-low) */

/**
 * @brief Initialize I2S audio output (I2S1 master, 16kHz/16-bit stereo)
 * @return ESP_OK on success
 */
esp_err_t audio_init(void);

/**
 * @brief Initialize the amplifier control GPIO
 * @return ESP_OK on success
 */
esp_err_t audio_ctrl_init(void);

/**
 * @brief Enable or disable the audio amplifier
 * @param state true=enable (amp on), false=disable (amp off/muted)
 * @return ESP_OK on success
 */
esp_err_t audio_set_amp(bool state);

/**
 * @brief Play a WAV file from SD card (blocking)
 * @param filepath Full path, e.g. "/sdcard/sounds/armed.wav"
 * @return ESP_OK on success
 */
esp_err_t audio_play_wav(const char *filepath);

/**
 * @brief Set volume level (0-100)
 * @param volume Volume percentage (0=mute, 100=max)
 */
void audio_set_volume(uint8_t volume);

/**
 * @brief Get current volume level
 * @return Volume percentage (0-100)
 */
uint8_t audio_get_volume(void);

#endif
