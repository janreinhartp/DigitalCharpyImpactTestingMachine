#include "bsp_audio.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h"

static i2s_chan_handle_t tx_chan = NULL;
static uint8_t current_volume = 80;  /* Default 80% */

esp_err_t audio_init(void)
{
    esp_err_t err = ESP_OK;

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_1,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 256,
        .auto_clear = true,
        .intr_priority = 0,
    };
    err = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (err != ESP_OK) {
        AUDIO_ERROR("Failed to create I2S channel: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = 16000,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = AUDIO_GPIO_BCLK,
            .ws = AUDIO_GPIO_LRCLK,
            .dout = AUDIO_GPIO_SDATA,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    err = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (err != ESP_OK) {
        AUDIO_ERROR("Failed to init I2S std mode: %s", esp_err_to_name(err));
        return err;
    }

    err = i2s_channel_enable(tx_chan);
    if (err != ESP_OK) {
        AUDIO_ERROR("Failed to enable I2S channel: %s", esp_err_to_name(err));
        return err;
    }

    AUDIO_INFO("I2S audio initialized (16kHz, 16-bit, stereo)");
    return ESP_OK;
}

esp_err_t audio_ctrl_init(void)
{
    const gpio_config_t gpio_cfg = {
        .pin_bit_mask = 1ULL << AUDIO_GPIO_CTRL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&gpio_cfg);
    if (err != ESP_OK) {
        AUDIO_ERROR("Failed to configure amp ctrl GPIO");
        return err;
    }

    /* Start with amp disabled (high = off, active-low) */
    gpio_set_level(AUDIO_GPIO_CTRL, 1);
    AUDIO_INFO("Audio amplifier control initialized on IO%d", AUDIO_GPIO_CTRL);
    return ESP_OK;
}

esp_err_t audio_set_amp(bool state)
{
    /* Active-low: low = amp on, high = amp off */
    gpio_set_level(AUDIO_GPIO_CTRL, state ? 0 : 1);
    return ESP_OK;
}

static bool validate_wav_header(FILE *file)
{
    if (file == NULL)
        return false;

    long original_pos = ftell(file);
    if (fseek(file, 0, SEEK_SET) != 0)
        return false;

    uint8_t header[44];
    if (fread(header, 1, 44, file) != 44) {
        fseek(file, original_pos, SEEK_SET);
        return false;
    }

    if (memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0 ||
        memcmp(header + 12, "fmt ", 4) != 0) {
        AUDIO_ERROR("Invalid WAV header");
        fseek(file, original_pos, SEEK_SET);
        return false;
    }

    uint16_t audio_format = *(uint16_t *)(header + 20);
    if (audio_format != 1) {
        AUDIO_ERROR("Only PCM WAV supported (got format %d)", audio_format);
        fseek(file, original_pos, SEEK_SET);
        return false;
    }

    uint16_t num_channels = *(uint16_t *)(header + 22);
    uint32_t sample_rate = *(uint32_t *)(header + 24);
    uint16_t bits_per_sample = *(uint16_t *)(header + 34);

    AUDIO_INFO("WAV: %d ch, %lu Hz, %d bit", num_channels, sample_rate, bits_per_sample);

    fseek(file, original_pos, SEEK_SET);
    return true;
}

esp_err_t audio_play_wav(const char *filepath)
{
    if (filepath == NULL || tx_chan == NULL)
        return ESP_ERR_INVALID_ARG;

    FILE *fh = fopen(filepath, "rb");
    if (fh == NULL) {
        AUDIO_ERROR("Failed to open %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    if (!validate_wav_header(fh)) {
        fclose(fh);
        return ESP_ERR_INVALID_ARG;
    }

    /* Skip 44-byte WAV header */
    if (fseek(fh, 44, SEEK_SET) != 0) {
        fclose(fh);
        return ESP_FAIL;
    }

    const size_t SAMPLES_PER_BUFFER = 512;
    const size_t INPUT_BUF_SIZE = SAMPLES_PER_BUFFER * sizeof(int16_t);
    const size_t OUTPUT_BUF_SIZE = SAMPLES_PER_BUFFER * sizeof(int16_t);

    int16_t *input_buf = heap_caps_malloc(INPUT_BUF_SIZE, MALLOC_CAP_SPIRAM);
    int16_t *output_buf = heap_caps_malloc(OUTPUT_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (input_buf == NULL || output_buf == NULL) {
        AUDIO_ERROR("Failed to allocate audio buffers");
        free(input_buf);
        free(output_buf);
        fclose(fh);
        return ESP_ERR_NO_MEM;
    }

    /* Volume scaling factor (0.0 - 10.0 range mapped from 0-100%) */
    int32_t vol_scale = (int32_t)current_volume * 10 / 100;
    if (vol_scale < 1 && current_volume > 0) vol_scale = 1;

    audio_set_amp(true);

    size_t samples_read, bytes_written;
    esp_err_t err = ESP_OK;

    while ((samples_read = fread(input_buf, sizeof(int16_t), SAMPLES_PER_BUFFER, fh)) > 0) {
        for (size_t i = 0; i < samples_read; i++) {
            int32_t sample = (int32_t)input_buf[i] * vol_scale;
            if (sample > 32767) sample = 32767;
            else if (sample < -32768) sample = -32768;
            output_buf[i] = (int16_t)sample;
        }
        size_t bytes_to_write = samples_read * sizeof(int16_t);
        bytes_written = 0;
        err = i2s_channel_write(tx_chan, output_buf, bytes_to_write, &bytes_written, portMAX_DELAY);
        if (err != ESP_OK) {
            AUDIO_ERROR("I2S write failed: %s", esp_err_to_name(err));
            break;
        }
    }

    audio_set_amp(false);
    free(input_buf);
    free(output_buf);
    fclose(fh);
    return err;
}

void audio_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    current_volume = volume;
    AUDIO_INFO("Volume set to %u%%", current_volume);
}

uint8_t audio_get_volume(void)
{
    return current_volume;
}
