#ifndef _DBTT_MANAGER_H_
#define _DBTT_MANAGER_H_

#include "esp_err.h"
#include "charpy_calc.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of individual tests in a single DBTT session */
#define DBTT_SESSION_MAX_TESTS  20

/**
 * @brief One data point collected during a DBTT session.
 *        Temperature is entered by the operator before each test;
 *        energy and angle come from the Charpy test result.
 */
typedef struct {
    float temperature_c;    /**< Specimen temperature (°C) */
    float energy_joules;    /**< Absorbed impact energy (J) */
    float final_angle_deg;  /**< Post-impact pendulum angle (°) */
    char  specimen_id[24];  /**< Auto-generated ID, e.g. "DBTT-3" */
} dbtt_point_t;

/**
 * @brief Complete DBTT test session descriptor.
 */
typedef struct {
    char  material[32];     /**< Material type from setup dropdown */
    char  operator_name[32];/**< Operator name entered at setup */
    float width_mm;         /**< Specimen width (mm) — shared for all tests */
    float height_mm;        /**< Specimen height (mm) */
    float length_mm;        /**< Specimen length (mm) */
    int   n_planned;        /**< Total number of tests planned */
    int   n_done;           /**< Number of tests completed so far */
    dbtt_point_t points[DBTT_SESSION_MAX_TESTS]; /**< Results array */
} dbtt_session_t;

/**
 * @brief Start a new DBTT session. Resets any previous session data.
 *
 * @param material      Material type string
 * @param operator_name Operator name string
 * @param n_planned     Number of tests planned (clamped to DBTT_SESSION_MAX_TESTS)
 * @param width_mm      Specimen width in mm
 * @param height_mm     Specimen height in mm
 * @param length_mm     Specimen length in mm
 */
void dbtt_manager_start(const char *material, const char *operator_name,
                        int n_planned,
                        float width_mm, float height_mm, float length_mm);

/**
 * @brief Abort / reset the current session. Safe to call at any time.
 */
void dbtt_manager_reset(void);

/**
 * @brief Returns true if a session is in progress (n_done < n_planned).
 */
bool dbtt_manager_is_active(void);

/**
 * @brief Record one completed Charpy test into the DBTT session.
 *        Extracts temperature_c, energy_joules, final_angle_deg, specimen_id
 *        directly from the test_result_t.
 *        Automatically marks session complete when n_done reaches n_planned.
 *
 * @param result  Pointer to the completed test result (must not be NULL)
 */
void dbtt_manager_record_result(const test_result_t *result);

/**
 * @brief Get a read-only pointer to the current session descriptor.
 *        Valid even after the session is complete (until next reset or start).
 */
const dbtt_session_t *dbtt_manager_get_session(void);

/**
 * @brief Append session data to /sdcard/DBTT_results.csv.
 *        Each call appends a new session block (header + rows).
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if SD not mounted, ESP_FAIL on I/O error
 */
esp_err_t dbtt_manager_save_csv(void);

/** Maximum saved sessions that dbtt_manager_list_sessions() can return */
#define DBTT_MAX_SESSIONS  20

/**
 * @brief Save the current session as a timestamped binary file on the SD card.
 *        File: /sdcard/DBTT_YYYYMMDD_HHMMSS.dat
 *        Called automatically when the session completes and by the result screen.
 *
 * @return ESP_OK on success
 */
esp_err_t dbtt_manager_save_session(void);

/**
 * @brief List saved DBTT session binary files on the SD card (DBTT_*.dat),
 *        sorted alphabetically (oldest first).
 *
 * @param list  Array of char[32] to receive filenames (base name only)
 * @param max   Size of the list array
 * @return Number of sessions found
 */
int dbtt_manager_list_sessions(char list[][32], int max);

/**
 * @brief Load a saved session from the SD card.
 *
 * @param filename  Base filename (e.g. "DBTT_20260612_143022.dat")
 * @param out       Destination struct (must not be NULL)
 * @return ESP_OK on success
 */
esp_err_t dbtt_manager_load_session(const char *filename, dbtt_session_t *out);

#ifdef __cplusplus
}
#endif

#endif /* _DBTT_MANAGER_H_ */
