#include "bsp_angle.h"
#include <math.h>

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static uint16_t zero_offset     = 0;
static float    scale_factor    = 0.5f;   /* pulley 2:1 — sensor rotates 2× per arm degree */
static bool     sensor_reversed = false;  /* true when sensor rotates opposite to arm */

/* EMA filter state — runs in ARM domain (signed degrees, ~-180..+180) */
static float  ema_angle       = 0.0f;
static bool   ema_initialized = false;
#define EMA_ALPHA  0.10f   /* 0=max smoothing, 1=no smoothing */
                           /* 0.10 → ~9 ms time constant at 1 kHz; adequate for
                            * settling detection while rejecting sensor noise */


esp_err_t angle_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id  = POT_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &s_adc_handle);
    if (err != ESP_OK) {
        ANGLE_ERROR("Failed to init ADC unit: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = POT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,  /* 12-bit on ESP32-P4 → 0–4095 */
    };
    err = adc_oneshot_config_channel(s_adc_handle, POT_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) {
        ANGLE_ERROR("Failed to configure ADC channel: %s", esp_err_to_name(err));
        return err;
    }

    ANGLE_INFO("Potentiometer ADC ready (ADC1_CH%d = GPIO%d)",
               POT_ADC_CHANNEL, POT_GPIO);
    return ESP_OK;
}

esp_err_t angle_read_raw(uint16_t *raw_angle)
{
    if (raw_angle == NULL || s_adc_handle == NULL)
        return ESP_ERR_INVALID_ARG;

    /* Oversample 16× and average to reduce ADC noise.
     * Single-read noise on a potentiometer is ~±40 counts (~±1.8°);
     * 16-sample averaging cuts it by √16 → ~±10 counts (~±0.5°). */
    int32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        int sample;
        esp_err_t err = adc_oneshot_read(s_adc_handle, POT_ADC_CHANNEL, &sample);
        if (err != ESP_OK)
            return err;
        sum += sample;
    }

    *raw_angle = (uint16_t)((sum / 16) & 0x0FFF);
    return ESP_OK;
}

esp_err_t angle_read_degrees(float *degrees)
{
    if (degrees == NULL)
        return ESP_ERR_INVALID_ARG;

    uint16_t raw;
    esp_err_t err = angle_read_raw(&raw);
    if (err != ESP_OK)
        return err;

    /* Signed count difference from zero.
     * Potentiometer is linear — no circular short-path wrapping needed.
     * adj is simply positive when pot is above zero, negative when below. */
    int32_t adj = sensor_reversed
                  ? (int32_t)zero_offset - (int32_t)raw
                  : (int32_t)raw        - (int32_t)zero_offset;

    /* Instant arm angle (signed, degrees) */
    float arm_instant = (float)adj * (360.0f / 4096.0f) * scale_factor;

    /* EMA low-pass filter (no circular wrap — linear sensor) */
    if (!ema_initialized) {
        ema_angle       = arm_instant;
        ema_initialized = true;
    } else {
        ema_angle += EMA_ALPHA * (arm_instant - ema_angle);
    }

    /* Output signed degrees relative to zero (positive or negative) */
    *degrees = ema_angle;
    return ESP_OK;
}

void angle_set_zero_offset(uint16_t offset)
{
    zero_offset = offset & 0x0FFF;
    /* Reset the EMA filter so it converges to the new zero immediately */
    ema_initialized = false;
    ANGLE_INFO("Zero offset set to %u (%.1f°)", zero_offset, (float)zero_offset * 360.0f / 4096.0f);
}

uint16_t angle_get_zero_offset(void)
{
    return zero_offset;
}

esp_err_t angle_calibrate_zero(void)
{
    uint16_t raw;
    esp_err_t err = angle_read_raw(&raw);
    if (err != ESP_OK)
        return err;

    angle_set_zero_offset(raw);
    ANGLE_INFO("Calibrated zero at raw=%u", raw);
    return ESP_OK;
}

/* ── Scale factor ──────────────────────────────────────────────────────── */

void angle_set_scale(float scale)
{
    sensor_reversed = (scale < 0.0f);
    scale_factor    = fabsf(scale);
    ema_initialized = false;
    ANGLE_INFO("Scale factor set to %.4f (%s)",
               scale_factor, sensor_reversed ? "reversed" : "forward");
}

float angle_get_scale(void)
{
    return scale_factor;
}

float angle_get_signed_scale(void)
{
    return sensor_reversed ? -scale_factor : scale_factor;
}

/* ── 3-point calibration ───────────────────────────────────────────────── */

typedef struct {
    uint16_t raw_zero;
    uint16_t raw_arm;
    float    arm_deg;
    uint16_t raw_max;
    float    max_deg;
    bool     zero_captured;
    bool     arm_captured;
    bool     max_captured;
} angle_cal_t;

static angle_cal_t s_cal;

/* Returns signed sensor degrees between a captured raw and the zero raw.
 * Potentiometer is linear — no circular wrapping. */
static float cal_signed_sensor_deg(uint16_t raw_val)
{
    int32_t adj = (int32_t)raw_val - (int32_t)s_cal.raw_zero;
    return (float)adj * 360.0f / 4096.0f;
}

esp_err_t angle_cal_capture_zero(void)
{
    uint16_t raw;
    esp_err_t err = angle_read_raw(&raw);
    if (err != ESP_OK) return err;
    s_cal.raw_zero      = raw;
    s_cal.zero_captured = true;
    ANGLE_INFO("Cal step 1 captured: raw=%u", raw);
    return ESP_OK;
}

uint16_t angle_cal_get_raw_zero(void)
{
    return s_cal.raw_zero;
}

esp_err_t angle_cal_capture_arm(float arm_deg)
{
    uint16_t raw;
    esp_err_t err = angle_read_raw(&raw);
    if (err != ESP_OK) return err;
    s_cal.raw_arm      = raw;
    s_cal.arm_deg      = arm_deg;
    s_cal.arm_captured = true;
    ANGLE_INFO("Cal step 2 captured: raw=%u, arm=%.1f°", raw, arm_deg);
    return ESP_OK;
}

esp_err_t angle_cal_capture_max(float arm_deg)
{
    uint16_t raw;
    esp_err_t err = angle_read_raw(&raw);
    if (err != ESP_OK) return err;
    s_cal.raw_max      = raw;
    s_cal.max_deg      = arm_deg;
    s_cal.max_captured = true;
    ANGLE_INFO("Cal step 3 captured: raw=%u, arm=%.1f°", raw, arm_deg);
    return ESP_OK;
}

esp_err_t angle_cal_apply(float *scale_out)
{
    if (!s_cal.zero_captured || !s_cal.arm_captured || !s_cal.max_captured) {
        ANGLE_ERROR("Cannot apply cal: not all points captured");
        return ESP_ERR_INVALID_STATE;
    }

    /* Helper: signed sensor degrees from zero for a captured raw value.
     * Takes the short path (±180°) so negative arm angles work correctly. */
    float sensor1 = cal_signed_sensor_deg(s_cal.raw_arm);
    float sensor2 = cal_signed_sensor_deg(s_cal.raw_max);

    float sf = 1.0f;
    int   n  = 0;
    float sf1 = 0.0f, sf2 = 0.0f;

    if (fabsf(sensor1) > 0.5f) { sf1 = s_cal.arm_deg / sensor1; n++; }
    if (fabsf(sensor2) > 0.5f) { sf2 = s_cal.max_deg / sensor2; n++; }

    if (n == 0) {
        ANGLE_ERROR("Cal points too close to zero — cannot compute scale");
        return ESP_ERR_INVALID_ARG;
    }
    sf = (n == 2) ? (sf1 + sf2) * 0.5f : (sf1 + sf2);   /* sum is safe: unused term is 0 */

    if (fabsf(sf) < 0.01f || fabsf(sf) > 100.0f) {
        ANGLE_ERROR("Computed scale %.4f out of plausible range", sf);
        return ESP_ERR_INVALID_ARG;
    }

    /* Commit: set zero offset then scale */
    angle_set_zero_offset(s_cal.raw_zero);
    angle_set_scale(sf);

    ANGLE_INFO("3-pt cal applied: zero_raw=%u  scale=%.4f  (sf1=%.4f sf2=%.4f)",
               s_cal.raw_zero, sf, sf1, sf2);

    if (scale_out) *scale_out = sf;
    return ESP_OK;
}

bool angle_cal_is_complete(void)
{
    return s_cal.zero_captured && s_cal.arm_captured && s_cal.max_captured;
}

esp_err_t angle_get_magnet_status(bool *detected, bool *too_strong, bool *too_weak)
{
    /* No magnet on a potentiometer — always report sensor as present. */
    if (detected   != NULL) *detected   = true;
    if (too_strong != NULL) *too_strong = false;
    if (too_weak   != NULL) *too_weak   = false;
    return ESP_OK;
}

esp_err_t angle_read_agc(uint8_t *agc)
{
    /* No AGC on a potentiometer — return a neutral mid-range value. */
    if (agc != NULL) *agc = 128;
    return ESP_OK;
}
