#include "audio_manager.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define AM_TAG "AUDIO_MGR"
#define AM_INFO(fmt, ...) ESP_LOGI(AM_TAG, fmt, ##__VA_ARGS__)
#define AM_ERROR(fmt, ...) ESP_LOGE(AM_TAG, fmt, ##__VA_ARGS__)

#define AUDIO_QUEUE_LENGTH  8
#define AUDIO_TASK_STACK    4096
#define AUDIO_TASK_PRIORITY 2

/* File paths for each sound event */
static const char *sound_files[SOUND_MAX] = {
    [SOUND_ARMED]     = "/sdcard/sounds/armed.wav",
    [SOUND_RELEASED]  = "/sdcard/sounds/released.wav",
    [SOUND_COMPLETE]  = "/sdcard/sounds/complete.wav",
    [SOUND_ABORT]     = "/sdcard/sounds/abort.wav",
    [SOUND_ERROR]     = "/sdcard/sounds/error.wav",
    [SOUND_TEST_BEEP] = "/sdcard/sounds/armed.wav",  /* Reuse armed beep for test */
};

static QueueHandle_t audio_queue = NULL;
static TaskHandle_t audio_task_handle = NULL;
static bool sounds_enabled = true;

static void audio_playback_task(void *arg)
{
    (void)arg;
    sound_event_t event;

    AM_INFO("Audio playback task started");

    while (1) {
        if (xQueueReceive(audio_queue, &event, portMAX_DELAY) == pdTRUE) {
            if (!sounds_enabled)
                continue;

            if (event >= SOUND_MAX) {
                AM_ERROR("Invalid sound event: %d", event);
                continue;
            }

            const char *filepath = sound_files[event];
            if (filepath == NULL)
                continue;

            AM_INFO("Playing: %s", filepath);
            esp_err_t err = audio_play_wav(filepath);
            if (err != ESP_OK) {
                AM_ERROR("Failed to play %s: %s", filepath, esp_err_to_name(err));
            }
        }
    }
}

esp_err_t audio_manager_init(void)
{
    audio_queue = xQueueCreate(AUDIO_QUEUE_LENGTH, sizeof(sound_event_t));
    if (audio_queue == NULL) {
        AM_ERROR("Failed to create audio queue");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ret = xTaskCreate(
        audio_playback_task,
        "audio_task",
        AUDIO_TASK_STACK,
        NULL,
        AUDIO_TASK_PRIORITY,
        &audio_task_handle);

    if (ret != pdPASS) {
        AM_ERROR("Failed to create audio task");
        return ESP_ERR_NO_MEM;
    }

    AM_INFO("Audio manager initialized");
    return ESP_OK;
}

esp_err_t audio_manager_play(sound_event_t event)
{
    if (audio_queue == NULL)
        return ESP_ERR_INVALID_STATE;

    if (!sounds_enabled)
        return ESP_OK;

    if (xQueueSend(audio_queue, &event, 0) != pdTRUE) {
        AM_ERROR("Audio queue full, dropping sound event %d", event);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void audio_manager_set_enabled(bool enabled)
{
    sounds_enabled = enabled;
    AM_INFO("Sound notifications %s", enabled ? "enabled" : "disabled");
}

bool audio_manager_is_enabled(void)
{
    return sounds_enabled;
}
