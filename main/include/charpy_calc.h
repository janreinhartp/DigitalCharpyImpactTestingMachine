#ifndef _CHARPY_CALC_H_
#define _CHARPY_CALC_H_

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Pendulum configuration parameters (stored in NVS)
 */
typedef struct {
    float mass_kg;          /* Pendulum mass in kg (e.g. 3.950) */
    float arm_length_m;     /* Arm pivot-to-center-of-gravity in meters (e.g. 0.400) */
    float release_angle_deg;/* Default release angle in degrees (e.g. 150.0) */
} pendulum_config_t;

/**
 * @brief Specimen information entered by operator
 */
typedef struct {
    char specimen_id[32];   /* e.g. "STEEL-043" */
    char material[32];      /* e.g. "Mild Steel" */
    float width_mm;         /* Specimen width in mm */
    float height_mm;        /* Specimen height in mm */
    float length_mm;        /* Specimen length in mm */
    float notch_depth_mm;   /* Notch depth through specimen height in mm */
    float temperature_c;    /* Test temperature in °C */
    char operator_name[32]; /* e.g. "J. Reyes" */
    char notes[64];         /* Optional notes */
} specimen_info_t;

/**
 * @brief Complete test result
 */
typedef struct {
    specimen_info_t specimen;
    float release_angle_deg;    /* Angle at release (alpha) */
    float final_angle_deg;      /* Angle after impact (beta) */
    float energy_joules;        /* Calculated absorbed energy */
    float impact_strength_j_cm2;/* Energy divided by net ligament area */
    bool impact_strength_valid; /* False for invalid geometry or legacy records */
    char timestamp[24];         /* "YYYY-MM-DD HH:MM:SS" */
} test_result_t;

/* Gravitational acceleration constant */
#define GRAVITY_M_S2  9.80665f

/**
 * @brief Calculate absorbed impact energy using the Charpy formula
 *
 * E = m * g * L * (cos(beta) - cos(alpha))
 *
 * @param mass_kg Pendulum mass in kg
 * @param arm_length_m Arm length in meters
 * @param release_angle_deg Release angle alpha in degrees
 * @param final_angle_deg Post-impact angle beta in degrees
 * @return Absorbed energy in Joules
 */
static inline float charpy_calc_energy(float mass_kg, float arm_length_m,
                                       float release_angle_deg, float final_angle_deg)
{
    float alpha_rad = release_angle_deg * (float)M_PI / 180.0f;
    float beta_rad  = final_angle_deg * (float)M_PI / 180.0f;
    return mass_kg * GRAVITY_M_S2 * arm_length_m * (cosf(beta_rad) - cosf(alpha_rad));
}

/**
 * @brief Calculate the remaining ligament area at the notch in cm²
 *
 * A = width_mm * (height_mm - notch_depth_mm) / 100
 *
 * @return true when the geometry is valid and area_cm2 was written
 */
static inline bool charpy_calc_net_area_cm2(float width_mm, float height_mm,
                                            float notch_depth_mm, float *area_cm2)
{
    if (area_cm2 == NULL) return false;
    *area_cm2 = 0.0f;

    if (!isfinite(width_mm) || !isfinite(height_mm) ||
        !isfinite(notch_depth_mm) || width_mm <= 0.0f || height_mm <= 0.0f ||
        notch_depth_mm < 0.0f || notch_depth_mm >= height_mm) {
        return false;
    }

    float calculated_area_cm2 = width_mm * (height_mm - notch_depth_mm) / 100.0f;
    if (!isfinite(calculated_area_cm2) || calculated_area_cm2 <= 0.0f) return false;

    *area_cm2 = calculated_area_cm2;
    return true;
}

/**
 * @brief Calculate Charpy impact strength from absorbed energy and geometry
 *
 * @return true when energy and geometry are valid and strength_j_cm2 was written
 */
static inline bool charpy_calc_impact_strength(float energy_joules,
                                               float width_mm,
                                               float height_mm,
                                               float notch_depth_mm,
                                               float *strength_j_cm2)
{
    float area_cm2;
    if (strength_j_cm2 == NULL) return false;
    *strength_j_cm2 = 0.0f;

    if (!isfinite(energy_joules) || energy_joules < 0.0f ||
        !charpy_calc_net_area_cm2(width_mm, height_mm, notch_depth_mm, &area_cm2)) {
        return false;
    }

    float calculated_strength = energy_joules / area_cm2;
    if (!isfinite(calculated_strength)) return false;

    *strength_j_cm2 = calculated_strength;
    return true;
}

/**
 * @brief Default pendulum configuration
 */
static inline pendulum_config_t charpy_default_config(void)
{
    pendulum_config_t cfg = {
        .mass_kg = 3.950f,
        .arm_length_m = 0.400f,
        .release_angle_deg = 150.0f,
    };
    return cfg;
}

#endif
