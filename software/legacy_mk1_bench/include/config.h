/**
 * @file config.h
 * @brief Project-specific hardware and motion configuration file.
 *
 * This is the main configuration file for the robot project. It defines
 * all hardware pins, system-wide settings (like the ISR frequency),
 * and the specific motion parameters for each axis.
 *
 * This version is configured for a FIXED-POINT math backend.
 */

#pragma once
#include <Arduino.h>
#include "stepper.h" // Include the library to get the StepperConfig struct definition

// ========================================================================
// 1. FIXED-POINT MATH DEFINITIONS
// These macros are used to convert standard numbers into the 32-bit
// fixed-point format (Q16.16) used by the controller.
// ========================================================================

// The number of fractional bits (16 bits for integer, 16 for fraction)
#define FX_SHIFT 16
// A fixed-point representation of 1.0 (1 << 16)
#define FX_ONE (1L << FX_SHIFT)
// A fixed-point representation of 0.5 (used for rounding)
#define FX_HALF (1L << (FX_SHIFT - 1))
// Converts a standard float into our fixed-point format
#define FX_FROM_FLOAT(f) ((int32_t)((f) * (float)FX_ONE))
// Converts a fixed-point value back to a float (for printing, etc.)
#define FX_TO_FLOAT(fx) ((float)(fx) / (float)FX_ONE)


// ========================================================================
// 2. SYSTEM-WIDE SETTINGS
// ========================================================================

/**
 * @brief ISR update frequency in Hertz (Hz).
 * Because we are using fast fixed-point math, we can dramatically
 * increase this frequency from the 4kHz float version.
 * 20kHz (50µs per tick) is a good starting point and unlocks
 * much higher motor speeds and serial stability.
 */
#define CONTROLLER_TICK_HZ 10000UL

/**
 * @brief Minimum step pulse width in ISR ticks.
 * At 20kHz, 1 tick is 50µs. A value of 1 is still fine.
 */
#define STEP_PULSE_TICKS 1

/**
 * @brief Logic level of a *triggered* endstop.
 * - LOW: For normally-open (NO) switches wired to GND. (Most common)
 * - HIGH: For normally-closed (NC) switches wired to GND, or NO to VCC.
 */
#define ENDSTOP_TRIGGERED LOW


// ========================================================================
// 3. HARDWARE PIN DEFINITIONS
// Map the physical pins of your microcontroller.
// (Unchanged)
// ========================================================================

// --- Pitch Axis ---
#define PITCH_STEP_PIN PD5
#define PITCH_DIR_PIN PD4
#define PITCH_ENDSTOP_PIN PD3

// --- Yaw Axis ---
#define YAW_STEP_PIN PD6
#define YAW_DIR_PIN PD7
#define YAW_ENDSTOP_PIN PD2


// ========================================================================
// 4. AXIS-SPECIFIC CONFIGURATIONS
// All float values are now int32_t and MUST be set using FX_FROM_FLOAT().
// ========================================================================

/**
 * @brief Configuration object for the Pitch axis.
 */
const StepperConfig pitchConfig = {
  // --- Motor Properties ---
  .steps_per_rev = (200 * 32), // Motor full steps * microsteps * gear ratio
  
  // --- Motion Limits (in degrees) ---
  .range_max_deg = FX_FROM_FLOAT(190.0f),            // Max allowed position
  .motion_lower_bound_deg = FX_FROM_FLOAT(0.0f),     // Min allowed position (after homing)
  
  // --- Homing Properties ---
  .home_pos_deg = FX_FROM_FLOAT(-30.0f),             // The logical position set at the endstop switch
  .homing_retract_deg = FX_FROM_FLOAT(30.0f),        // Distance to back off after first hit
  .homing_max_travel_deg = FX_FROM_FLOAT(360.0f),    // Safety limit to trigger a HOMING_FAILED error
  .homing_fine_speed_factor = FX_FROM_FLOAT(0.1f),   // Speed reduction for fine approach (10%)
  .home_dir = -1,                                    // Direction to move for homing (-1 or +1)
  
  // --- Hardware/System Links ---
  .endstop_active_level = ENDSTOP_TRIGGERED, // Logic level (LOW/HIGH)
  .step_pulse_ticks = STEP_PULSE_TICKS,      // Pulse width
  
  // --- Motion Profile Defaults ---
  // We can now safely use the original high-speed values
  .default_cruise_speed_dps = FX_FROM_FLOAT(500.0f), // deg/sec
  .default_max_accel_dps2 = FX_FROM_FLOAT(400.0f)    // deg/sec^2
};

/**
 * @brief Configuration object for the Yaw axis.
 */
const StepperConfig yawConfig = {
  // --- Motor Properties ---
  .steps_per_rev = (200 * 32),
  
  // --- Motion Limits (in degrees) ---
  .range_max_deg = FX_FROM_FLOAT(120.0f),
  .motion_lower_bound_deg = FX_FROM_FLOAT(0.0f),
  
  // --- Homing Properties ---
  .home_pos_deg = FX_FROM_FLOAT(-30.0f),
  .homing_retract_deg = FX_FROM_FLOAT(30.0f),
  .homing_max_travel_deg = FX_FROM_FLOAT(360.0f),
  .homing_fine_speed_factor = FX_FROM_FLOAT(0.1f),
  .home_dir = -1,
  
  // --- Hardware/System Links ---
  .endstop_active_level = ENDSTOP_TRIGGERED,
  .step_pulse_ticks = STEP_PULSE_TICKS,
  
  // --- Motion Profile Defaults ---
  .default_cruise_speed_dps = FX_FROM_FLOAT(500.0f), // deg/sec
  .default_max_accel_dps2 = FX_FROM_FLOAT(400.0f)    // deg/sec^2
};