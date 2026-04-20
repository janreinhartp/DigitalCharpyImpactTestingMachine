#ifndef _TEST_MANAGER_H_
#define _TEST_MANAGER_H_

#include "esp_err.h"
#include "charpy_calc.h"
#include <stdbool.h>

/**
 * @brief Test state machine states
 */
typedef enum {
    TEST_STATE_IDLE = 0,
    TEST_STATE_SPECIMEN_ENTRY,
    TEST_STATE_ARMING,
    TEST_STATE_ARMED,
    TEST_STATE_RELEASED,
    TEST_STATE_MEASURING,
    TEST_STATE_COMPLETE,
    TEST_STATE_LOGGING,
    TEST_STATE_ERROR,
} test_state_t;

/**
 * @brief State change callback
 */
typedef void (*test_state_cb_t)(test_state_t old_state, test_state_t new_state);

/**
 * @brief Initialize the test manager
 * @param config Pendulum configuration
 * @return ESP_OK on success
 */
esp_err_t test_manager_init(const pendulum_config_t *config);

/**
 * @brief Get current test state
 */
test_state_t test_manager_get_state(void);

/**
 * @brief Get state name as string
 */
const char *test_manager_state_name(test_state_t state);

/**
 * @brief Start a new test with specimen info (IDLE → SPECIMEN_ENTRY)
 */
esp_err_t test_manager_start_new(void);

/**
 * @brief Set specimen info and begin arming (SPECIMEN_ENTRY → ARMING)
 */
esp_err_t test_manager_arm(const specimen_info_t *specimen);

/**
 * @brief Release the arm (ARMED → RELEASED → MEASURING)
 */
esp_err_t test_manager_release(void);

/**
 * @brief Abort current test (returns to IDLE)
 */
esp_err_t test_manager_abort(void);

/**
 * @brief Save current result (COMPLETE → LOGGING → IDLE)
 */
esp_err_t test_manager_save_result(void);

/**
 * @brief Discard current result (COMPLETE → IDLE)
 */
esp_err_t test_manager_discard_result(void);

/**
 * @brief Get the current live angle reading (updated by sampling task)
 */
float test_manager_get_live_angle(void);

/**
 * @brief Get the last completed test result
 */
const test_result_t *test_manager_get_result(void);

/**
 * @brief Get current specimen info
 */
const specimen_info_t *test_manager_get_specimen(void);

/**
 * @brief Register callback for state changes (for UI updates)
 */
void test_manager_register_state_cb(test_state_cb_t cb);

/**
 * @brief Update pendulum configuration (runtime, also saves to NVS)
 */
esp_err_t test_manager_set_config(const pendulum_config_t *config);

/**
 * @brief Get current pendulum configuration
 */
const pendulum_config_t *test_manager_get_config(void);

/**
 * @brief Called by endstop ISR callback when arm reaches top position
 */
void test_manager_endstop_triggered(bool pressed);

#endif
