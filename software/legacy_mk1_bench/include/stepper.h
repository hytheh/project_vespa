/**
 * @file stepper.h
 * @brief A scalable, non-blocking, interrupt-driven Stepper Motor Controller library.
 *
 * This file defines the interface for the StepperController class.
 * This version uses 32-bit (Q16.16) fixed-point math for all internal
 * calculations, eliminating floating-point operations from the ISR for
 * maximum performance.
 */

#pragma once

#include <Arduino.h>

// Forward-declare the fixed-point macros.
// This allows stepper.h to be used without config.h
#ifndef FX_SHIFT
  #define FX_SHIFT 16
  #define FX_ONE (1L << FX_SHIFT)
  #define FX_HALF (1L << (FX_SHIFT - 1))
  #define FX_FROM_FLOAT(f) ((int32_t)((f) * (float)FX_ONE))
  #define FX_TO_FLOAT(fx) ((float)(fx) / (float)FX_ONE)
#endif

/**
 * @brief Holds the parameters for a new motion command.
 * All members are now 32-bit fixed-point integers.
 */
struct MotionTarget {
  int32_t target_deg;       // Target position (Q16.16)
  int32_t cruise_speed_dps; // Cruising speed (Q16.16)
  int32_t max_accel_dps2;   // Max acceleration (Q16.16)
  bool active;            // True if a motion is currently active
};

/**
 * @brief Error states for the controller. Can be polled by the main loop.
 * (Unchanged)
 */
enum StepperError {
  NO_ERROR,               // All good
  TARGET_OUT_OF_BOUNDS,   // A moveToDeg() command was clamped to limits
  HOMING_FAILED           // Homing move exceeded max travel without hitting endstop
};

/**
 * @brief Configuration struct holding all per-axis settings.
 * All values are now fixed-point (int32_t) or standard integers.
 */
struct StepperConfig {
  long steps_per_rev;             // (int) Total steps for 360 degrees
  int32_t range_max_deg;            // (Q16.16) Upper motion boundary
  int32_t motion_lower_bound_deg;   // (Q16.16) Lower motion boundary (usually 0.0)
  int32_t home_pos_deg;             // (Q16.16) Logical position at endstop
  int32_t homing_retract_deg;       // (Q16.16) Distance to back off switch
  int32_t homing_max_travel_deg;    // (Q16.16) Safety limit for homing
  int32_t homing_fine_speed_factor; // (Q16.16) e.g., FX_FROM_FLOAT(0.1)
  int8_t home_dir;                // (int) Homing direction (+1 or -1)
  bool endstop_active_level;      // (bool) The digital level of a *triggered* switch
  uint8_t step_pulse_ticks;       // (int) Min step pulse width in ISR ticks
  int32_t default_cruise_speed_dps; // (Q16.16) Fallback speed
  int32_t default_max_accel_dps2;   // (Q16.16) Fallback acceleration
};

/**
 * @brief The main Stepper Controller class.
 * Each instance controls one stepper motor.
 */
class StepperController {
public:
  /**
   * @brief Constructor.
   * @param stepPin The microcontroller pin for STEP.
   * @param dirPin The microcontroller pin for DIRECTION.
   * @param endstopPin The microcontroller pin for the endstop switch.
   * @param config A const reference to the StepperConfig struct for this axis.
   * @param controller_tick_hz The system's ISR frequency (from config.h).
   *
   * Automatically registers this instance with the ISR update list.
   */
  StepperController(uint8_t stepPin, uint8_t dirPin, uint8_t endstopPin,
                    const StepperConfig& config, uint32_t controller_tick_hz);

  /**
   * @brief Initializes the hardware pins (call this in setup()).
   */
  void begin();

  /**
   * @brief Starts the non-blocking homing sequence.
   * @param homing_speed_dps (float) Optional speed. If not provided,
   * uses default_cruise_speed_dps from config.
   */
  void startHoming(float homing_speed_dps = -1.0f);

  /**
   * @brief Checks if the homing sequence is currently active.
   * @return true if homing, false otherwise.
   */
  bool isHoming() const;
  
  /**
   * @brief Checks if the axis has been successfully homed.
   * @return true if homed, false otherwise.
   */
  bool isHomed() const { return homed; }

  /**
   * @brief Checks if the motor is currently executing a move.
   * @return true if moving, false otherwise.
   */
  bool isMoving() const;

  /**
   * @brief Schedules a new non-blocking move to an absolute position.
   * The motion will be executed by the ISR using a trapezoidal profile.
   * @param deg (float) The absolute target position in degrees.
   * @param cruise_speed_dps (float) Optional speed. Uses config default if not provided.
   * @param max_accel_dps2 (float) Optional acceleration. Uses config default if not provided.
   */
  void moveToDeg(float deg, float cruise_speed_dps = -1.0f,
                 float max_accel_dps2 = -1.0f);

  /**
   * @brief Rotates the motor continuously at a given speed and direction.
   * This is non-blocking. Any other motion call (moveToDeg, startHoming, stop)
   * will cancel the continuous spin.
   * @param speed_dps (float) The target speed in degrees/sec. Defaults to config default.
   * @param dir (int8_t) The direction (+1 for positive, -1 for negative). Defaults to +1.
   */
  void spin(float speed_dps = -1.0f, int8_t dir = 1);

  /**
   * @brief Immediately stops all motion and clears the active target.
   */
  void stop();

  /**
   * @brief Gets the current theoretical position of the motor.
   * @return (float) The position in degrees.
   */
  float getPositionDeg() const { return FX_TO_FLOAT(position_deg); }

  /**
   * @brief [ADVANCED] The core update function.
   * This is called by the ISR (via updateAll) at CONTROLLER_TICK_HZ.
   * **Do not call this from your main loop.**
   */
  void update();

  /**
   * @brief Gets the last error that occurred.
   * @return The StepperError enum value.
   */
  StepperError getError() const { return last_error; }

  /**
   * @brief Clears the current error state (acknowledges the error).
   */
  void clearError() { last_error = NO_ERROR; }

  /**
   * @brief [STATIC] Called by the ISR to update all registered controllers.
   * This function iterates the static linked list and calls update() on each
   * StepperController instance.
   */
  static void updateAll();

private:
  // --- Private Methods ---
  void stepPulse();             // Generates the STEP pulse
  void setDir(bool dir);        // Sets the DIRECTION pin
  
  /**
   * @brief Converts a fixed-point degree value to an integer step count.
   * This now uses integer-only math for high speed.
   * @param deg_fx (int32_t) A Q16.16 fixed-point degree value.
   * @return (long) The corresponding number of microsteps.
   */
  inline long degToSteps(int32_t deg_fx) const {
    // This calculates (deg_fx * steps_per_rev) / 360
    // We use a 64-bit intermediate to prevent overflow
    // (Q16.16 * Q0) = Q16.16
    int64_t temp = (int64_t)deg_fx * config.steps_per_rev;
    // (Q16.16 / Q0) = Q16.16
    temp /= 360;
    // Add 0.5 (in Q16.16) for rounding, then shift back to Q0 (integer)
    return (long)((temp + FX_HALF) >> FX_SHIFT);
  }

  // --- Hardware Pins ---
  const uint8_t stepPin;
  const uint8_t dirPin;
  const uint8_t endstopPin;

  // --- Injected Configuration ---
  const StepperConfig config;       // A copy of all settings for this axis
  const uint32_t tick_hz;           // The system's ISR frequency

  // --- Pre-calculated Fixed-Point Constants ---
  const int32_t dt_fx;                // (Q16.16) The time delta (1.0 / tick_hz)
  const int32_t deg_per_step_fx;      // (Q16.16) Smallest move angle (360.0 / steps_per_rev)

  // --- Homing State ---
  enum HomingPhase {
    HOMING_IDLE,      // Not homing
    HOMING_COARSE,    // Moving fast towards switch
    HOMING_RETRACT,   // Moving away from switch
    HOMING_FINE,      // Moving slow towards switch
    HOMING_MOVETO_ZERO// Using main planner to move to 0.0
  };
  volatile HomingPhase homing_phase; // The current phase of the homing state machine

  // --- Volatile State Variables (ALL NOW FIXED-POINT) ---
  // These are 'volatile' because they are modified by the ISR
  // and read by the main loop (or vice-versa).
  volatile int32_t position_deg;       // (Q16.16) Current "theory" position
  volatile int32_t velocity_dps;       // (Q16.16) Current "theory" velocity
  volatile bool homing_active;       // True if homing state machine is running
  volatile bool homed;               // True once homing is complete
  volatile bool continuous_rotation; // True if continuous mode is active
  volatile MotionTarget target;      // The currently active motion command (Q16.16)
  volatile long microstep_counter;   // Current "reality" position in microsteps
  volatile uint8_t step_pulse_ticks_remaining; // For timing the step pulse width
  volatile StepperError last_error;  // The last error that occurred
  volatile int32_t step_increment_fx;          // The fixed-point (Q16.16) number of steps to add per tick.
  volatile int32_t spin_accumulator_fx;        // The accumulator for the DDA. (When exceeding 1.0 (FX_ONE), a step is taken.)

  // --- Static Linked List ---
  // This enables a scalable number of controllers
  StepperController* next_stepper;          // Pointer to the next stepper in the list
  static StepperController* g_list_head;    // Static "head" of the list
};

// ========================================================================
// ISR TIMER MANAGEMENT
// ========================================================================

/**
 * @brief Configures the hardware timer (Timer1) to fire the ISR.
 * @param tick_hz The desired frequency (e.g., CONTROLLER_TICK_HZ).
 */
void setupTimerInterrupt(uint32_t tick_hz);