#ifndef _CHARPY_CALC_H_
#define _CHARPY_CALC_H_

#include <math.h>
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
