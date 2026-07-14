/**
 * @file stepper.cpp
 * @brief Implementation of the StepperController (Fixed-Point Version).
 *
 * This file contains the logic for the motion planner, homing state
 * machine, and ISR management. It is designed to be fully generic
 * and should not be modified for project-specific needs.
 *
 * All motion logic in this file (especially in update()) is
 * implemented using fast integer-only fixed-point math.
 */

#include "stepper.h"

// Initialize the static head of the linked list.
// This pointer is shared by all instances of the class.
StepperController* StepperController::g_list_head = nullptr;

// ------------------------------------------------------------------------
// StepperController Public Method Implementations
// ------------------------------------------------------------------------

StepperController::StepperController(uint8_t stepPin_, uint8_t dirPin_, uint8_t endstopPin_,
                                     const StepperConfig& config_, uint32_t controller_tick_hz)
  : stepPin(stepPin_), dirPin(dirPin_), endstopPin(endstopPin_),
    config(config_),  // Copy the config struct into the class member
    tick_hz(controller_tick_hz),
    // Pre-calculate fixed-point constants for efficiency
    dt_fx(FX_FROM_FLOAT(1.0f / (float)controller_tick_hz)),
    deg_per_step_fx(FX_FROM_FLOAT(360.0f) / config.steps_per_rev)
{
    // Initialize all state variables from config (now fixed-point)
    position_deg = config.home_pos_deg; // Start at the "home" position logically
    microstep_counter = degToSteps(position_deg); // Uses new fixed-point degToSteps
    velocity_dps = 0L; // Use 0L for long (int32_t)
    homing_active = false;
    homed = false;
    homing_phase = HOMING_IDLE;
    continuous_rotation = false;
    step_pulse_ticks_remaining = 0;
    target.active = false;
    target.target_deg = 0L;
    target.cruise_speed_dps = config.default_cruise_speed_dps;
    target.max_accel_dps2 = config.default_max_accel_dps2;
    last_error = NO_ERROR;
    step_increment_fx = 0L;
    spin_accumulator_fx = 0L;

    // === Scalable ISR Registration ===
    // Add this new object to the front of the linked list.
    // This automatically registers it for ISR updates.
    this->next_stepper = g_list_head;
    g_list_head = this;
}

void StepperController::begin() {
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  // Use INPUT_PULLUP for the endstop; it's the most reliable way
  pinMode(endstopPin, INPUT_PULLUP); 
  digitalWrite(stepPin, LOW);
  digitalWrite(dirPin, LOW);
}

void StepperController::startHoming(float homing_speed_dps) {
  noInterrupts();
  continuous_rotation = false; // Stop any continuous spin
  homing_active = true;
  homed = false;
  homing_phase = HOMING_COARSE;
  target.active = true;

  // Convert float input to fixed-point
  target.cruise_speed_dps = (homing_speed_dps < 0) ? config.default_cruise_speed_dps : FX_FROM_FLOAT(homing_speed_dps);
  target.max_accel_dps2 = config.default_max_accel_dps2;

  // Reset logical position to the starting point
  position_deg = config.home_pos_deg;
  microstep_counter = degToSteps(position_deg);
  interrupts();
}

void StepperController::moveToDeg(float deg, float cruise_speed_dps, float max_accel_dps2) {
  // Convert float input to fixed-point
  int32_t deg_fx = FX_FROM_FLOAT(deg);

  // Clamp the target to the configured motion limits (now fixed-point)
  if (deg_fx < config.motion_lower_bound_deg) {
    last_error = TARGET_OUT_OF_BOUNDS;
    deg_fx = config.motion_lower_bound_deg;
  }
  if (deg_fx > config.range_max_deg) {
    last_error = TARGET_OUT_OF_BOUNDS;
    deg_fx = config.range_max_deg;
  }

  // Disable interrupts temporarily to make the update "atomic"
  noInterrupts();
  continuous_rotation = false; // Stop any continuous spin
  homing_active = false;       // Stop any homing
  target.target_deg = deg_fx;
  // Convert float inputs to fixed-point
  target.cruise_speed_dps = (cruise_speed_dps < 0) ? config.default_cruise_speed_dps : FX_FROM_FLOAT(cruise_speed_dps);
  target.max_accel_dps2 = (max_accel_dps2 < 0) ? config.default_max_accel_dps2 : FX_FROM_FLOAT(max_accel_dps2);
  target.active = true; // This "activates" the motion planner in the ISR
  interrupts();
}

void StepperController::spin(float speed_dps, int8_t dir) {
  // === OPTIMIZED SPIN CALCULATION ===
  // This function is called ONCE, so we can use slow math here.
  // The ISR will use the pre-calculated results for high speed.
  
  // 1. Determine speed in degrees/sec (float)
  float speed_float = (speed_dps < 0) ? FX_TO_FLOAT(config.default_cruise_speed_dps) : speed_dps;
  float speed_with_dir = speed_float * (float)dir;

  // 2. Convert to steps/sec (float)
  float v_sps = speed_with_dir * (float)config.steps_per_rev / 360.0f;

  // 3. Convert to steps/tick (float)
  float steps_per_tick = v_sps / (float)tick_hz;

  noInterrupts();
  // Stop all other motion
  homing_active = false;
  target.active = false;
  
  continuous_rotation = true; // Set the continuous spin state
  
  // 4. Store the pre-calculated increment in fixed-point
  // This is the number of steps (as a Q16.16) to add per tick
  step_increment_fx = FX_FROM_FLOAT(steps_per_tick);
  
  // Store the "official" velocity (for getPositionDeg)
  velocity_dps = FX_FROM_FLOAT(speed_with_dir);
  
  // Reset the accumulator
  spin_accumulator_fx = 0;
  
  interrupts();
}

void StepperController::stop() {
  noInterrupts();
  target.active = false;
  continuous_rotation = false;
  homing_active = false;
  velocity_dps = 0L; // Force velocity to zero (fixed-point)
  step_increment_fx = 0L; // Stop the accumulator
  interrupts();
}

bool StepperController::isMoving() const {
  // The motor is "moving" if a target is active, homing is active, OR spinning
  return (bool)target.active || (bool)homing_active || (bool)continuous_rotation;
}

bool StepperController::isHoming() const {
  return (bool)homing_active;
}

// ------------------------------------------------------------------------
// StepperController Private Methods
// (Unchanged)
// ------------------------------------------------------------------------

void StepperController::setDir(bool dir) {
  // This could be customized to invert direction if a motor is wired backwards
  digitalWrite(dirPin, dir ? HIGH : LOW);
}

void StepperController::stepPulse() {
  // Set the pin HIGH. The 'update' function will set it LOW
  // after 'step_pulse_ticks' have passed.
  digitalWrite(stepPin, HIGH);
  step_pulse_ticks_remaining = config.step_pulse_ticks;
}

// ------------------------------------------------------------------------
// StepperController::update() - THE CORE LOGIC (FIXED-POINT)
// ------------------------------------------------------------------------

/**
 * @brief This is the heart of the controller.
 * It is called from the ISR at CONTROLLER_TICK_HZ.
 * All logic here is high-speed, integer-only, fixed-point math.
 */
void StepperController::update() {
  // --- 1. Manage Step Pulse Timing ---
  // If a pulse is active, count down. When it hits 0, pull the pin LOW.
  if (step_pulse_ticks_remaining > 0) {
    step_pulse_ticks_remaining--;
    if (step_pulse_ticks_remaining == 0) {
      digitalWrite(stepPin, LOW);
    }
  }

  // --- 2. Check Hard Limits ---
  // Safety stop if we ever exceed the max physical range (but not in spin mode)
  if (!continuous_rotation && position_deg >= config.range_max_deg) {
    stop();
  }
  // Symmetric lower hard-limit. Enforced only once homed, so it does not fire
  // during the homing sweep (which legitimately travels through/below 0).
  if (!continuous_rotation && homed && position_deg <= config.motion_lower_bound_deg) {
    stop();
  }

  // Read the endstop *once* per update tick
  bool endstop_hit = (digitalRead(endstopPin) == config.endstop_active_level);

  // --- 3. Continuous Spin Logic ---
  if (continuous_rotation) {
    // If in spin mode, just execute the stepping logic at the set velocity
    // No trapezoids, no targets, just spin.
    
    // Fixed-point position update:
    // position_deg += velocity_dps * dt_fx
    // (Q16.16) += (Q16.16 * Q16.16) >> 16
    // Use DDA accumulation to avoid using a 64-bit intermediate to prevent overflow during multiplication
    
    // 1. Update "theory" position (for getPositionDeg())
    // This is the *only* 64-bit op in this loop.
    // (Q16.16) += (Q16.16 * Q16.16) >> 16
    position_deg += (int32_t)(((int64_t)velocity_dps * dt_fx) >> FX_SHIFT);

    // 2. Update the high-speed step accumulator
    // (Q16.16) += (Q16.16)
    spin_accumulator_fx += step_increment_fx;
    
    // 3. Check if a step is due
    // (Q16.16) >= (Q16.16)
    if (spin_accumulator_fx >= FX_ONE) {
      spin_accumulator_fx -= FX_ONE; // Subtract 1.0 (fixed-point)
      setDir(velocity_dps > 0);
      stepPulse();
      microstep_counter += (velocity_dps > 0 ? 1 : -1);
    } else if (spin_accumulator_fx <= -FX_ONE) {
      // Handle negative increment
      spin_accumulator_fx += FX_ONE; // Add 1.0 (fixed-point)
      setDir(velocity_dps > 0);
      stepPulse();
      microstep_counter += (velocity_dps > 0 ? 1 : -1);
    }
    return; // Skip all other (slow) motion logic
  }

  // --- 4. Homing State Machine ---
  if (homing_active) {
    switch(homing_phase) {
        
        case HOMING_COARSE: {
            // Move fast towards the endstop
            int32_t target_velocity = (int32_t)config.home_dir * target.cruise_speed_dps;
            // accel_per_tick = accel * dt
            // (Q16.16) = (Q16.16 * Q16.16) >> 16
            int32_t max_a_tick = (int32_t)(((int64_t)target.max_accel_dps2 * dt_fx) >> FX_SHIFT);

            // Ramp up/down to target velocity (all fixed-point)
            if (config.home_dir > 0) { // Moving positive
                if (velocity_dps < target_velocity) { velocity_dps += max_a_tick; if (velocity_dps > target_velocity) velocity_dps = target_velocity; }
            } else { // Moving negative
                if (velocity_dps > target_velocity) { velocity_dps -= max_a_tick; if (velocity_dps < target_velocity) velocity_dps = target_velocity; }
            }
            
            // --- Stepping Logic ---
            position_deg += (int32_t)(((int64_t)velocity_dps * dt_fx) >> FX_SHIFT);
            long target_microsteps = degToSteps(position_deg);
            if (target_microsteps > microstep_counter) { setDir(HIGH); stepPulse(); microstep_counter++; }
            else if (target_microsteps < microstep_counter) { setDir(LOW); stepPulse(); microstep_counter--; }

            if(endstop_hit) {
                // Switch hit! Stop and move to next phase.
                homing_phase = HOMING_RETRACT;
                velocity_dps = 0L; // 0 in fixed-point
                target.active = true;
                // Set new target to retract from the switch (fixed-point)
                target.target_deg = position_deg - ((int32_t)config.home_dir * config.homing_retract_deg);
            }

            // Safety check: Has it traveled too far? (fixed-point)
            // Use labs() (long absolute value) for int32_t
            int32_t travel_dist = labs(position_deg - config.home_pos_deg);
            if (travel_dist > config.homing_max_travel_deg) {
                stop();
                homing_active = false;
                homing_phase = HOMING_IDLE;
                last_error = HOMING_FAILED;
            }
          }
          break;

        case HOMING_RETRACT: {
            // Move away from the endstop by a fixed amount
            int32_t error = target.target_deg - position_deg;
            // This is a simple proportional controller
            int8_t dir = (error >= 0L) ? 1 : -1; // Use 0L for long
            int32_t max_a = (int32_t)(((int64_t)target.max_accel_dps2 * dt_fx) >> FX_SHIFT);
            velocity_dps += (int32_t)dir * max_a;
            
            // Clamp velocity
            if(dir > 0 && velocity_dps > target.cruise_speed_dps) velocity_dps = target.cruise_speed_dps;
            if(dir < 0 && velocity_dps < -target.cruise_speed_dps) velocity_dps = -target.cruise_speed_dps;

            // --- Stepping Logic ---
            position_deg += (int32_t)(((int64_t)velocity_dps * dt_fx) >> FX_SHIFT);
            long target_microsteps = degToSteps(position_deg);
            if (target_microsteps > microstep_counter) { setDir(HIGH); stepPulse(); microstep_counter++; }
            else if (target_microsteps < microstep_counter) { setDir(LOW); stepPulse(); microstep_counter--; }

            // Check if arrived (within one step, using pre-calculated fixed-point value)
            if(labs(error) < deg_per_step_fx) {
                homing_phase = HOMING_FINE; // Move to next phase
                velocity_dps = 0L;
                target.active = true;
                // Slow down: cruise = cruise * factor (fixed-point)
                target.cruise_speed_dps = (int32_t)(((int64_t)target.cruise_speed_dps * config.homing_fine_speed_factor) >> FX_SHIFT);
            }
          }
          break;

        case HOMING_FINE: {
            // Move slowly back to the endstop (identical logic to COARSE)
            int32_t target_velocity = (int32_t)config.home_dir * target.cruise_speed_dps;
            int32_t max_a_tick = (int32_t)(((int64_t)target.max_accel_dps2 * dt_fx) >> FX_SHIFT);
            
            if (config.home_dir > 0) {
                if (velocity_dps < target_velocity) { velocity_dps += max_a_tick; if (velocity_dps > target_velocity) velocity_dps = target_velocity; }
            } else {
                if (velocity_dps > target_velocity) { velocity_dps -= max_a_tick; if (velocity_dps < target_velocity) velocity_dps = target_velocity; }
            }

            // --- Stepping Logic ---
            position_deg += (int32_t)(((int64_t)velocity_dps * dt_fx) >> FX_SHIFT);
            long target_microsteps = degToSteps(position_deg);
            if (target_microsteps > microstep_counter) { setDir(HIGH); stepPulse(); microstep_counter++; }
            else if (target_microsteps < microstep_counter) { setDir(LOW); stepPulse(); microstep_counter--; }

            if(endstop_hit) {
                // === HOMING COMPLETE ===
                homing_phase = HOMING_MOVETO_ZERO; // Final move
                velocity_dps = 0L;
                
                // This is the "magic": define the current physical
                // location as the logical 'home_pos_deg' (e.g., -30.0)
                position_deg = config.home_pos_deg;
                microstep_counter = degToSteps(position_deg);
                
                // Now, use the *regular* motion planner to move to 0.0
                target.target_deg = config.motion_lower_bound_deg;
                target.cruise_speed_dps = config.default_cruise_speed_dps;
                target.max_accel_dps2 = config.default_max_accel_dps2;
                target.active = true;
            }
          }
          break;

        case HOMING_MOVETO_ZERO:
          // In this phase, we are waiting for the "Regular Motion"
          // planner to finish its move to 0.0.
          // The check for completion is handled in the
          // "Regular Motion" block (see below).
          break;
        
        case HOMING_IDLE:
        default:
          // Homing is done or has failed, stop the state machine.
          homing_active = false;
          homed = true; // Flag to the main loop that we are done
          target.active = false;
          break;
    }
    
    // CRITICAL: If we are in the HOMING_MOVETO_ZERO phase, we must
    // *fall through* to the regular motion planner to execute the move.
    // For all other homing phases, we are done for this tick.
    if (homing_phase != HOMING_MOVETO_ZERO) {
        return;
    }
  }

  // --- 5. Regular Motion Planner ---
  
  // If no target is active, there's nothing to do.
  if (!target.active) {
    // Check if we just *finished* the final homing move
    if (homing_active && homing_phase == HOMING_MOVETO_ZERO) {
        homing_phase = HOMING_IDLE; // Transition to the final "done" state
    }
    return; // Nothing else to do
  }

  // --- Trapezoidal Motion Logic (Fixed-Point) ---
  int32_t error = target.target_deg - position_deg;
  int8_t dir = (error >= 0L) ? 1 : -1;

  // Check if we have arrived (within one step)
  if (labs(error) < deg_per_step_fx) {
    target.active = false; // We're done
    velocity_dps = 0L;
    return;
  }

  // --- Calculate Acceleration/Deceleration ---
  int32_t a_mag = target.max_accel_dps2;
  
  // 1. Calculate the distance needed to stop: stopping_dist = (v*v) / (2*a)
  // (Q16.16) = ( (Q16.16 * Q16.16) / (Q16.16 * Q0) )
  // We use int64_t to hold the intermediate (Q32.32) v*v value.
  int64_t v_squared = (int64_t)velocity_dps * velocity_dps; // This is Q32.32
  // We need to shift it back to Q16.16 *before* the division
  int32_t stopping_dist = (int32_t)((v_squared >> FX_SHIFT) / (a_mag * 2));

  // Ensure stopping_dist is positive (handles negative velocity)
  if (((int64_t)velocity_dps * dir) < 0L) {
    stopping_dist = 0L; // Moving away from target, needs to accel
  }

  // 2. Decide whether to accelerate or decelerate
  if (labs(error) <= stopping_dist) {
    // --- Deceleration Phase ---
    // We are in the "stopping zone," so decelerate
    // dv = -dir * a * dt
    int32_t dv = -dir * (int32_t)(((int64_t)a_mag * dt_fx) >> FX_SHIFT);
    velocity_dps += dv;
  } else {
    // --- Acceleration/Cruise Phase ---
    // We are far from the target, accelerate towards cruise speed
    int32_t v_limit = target.cruise_speed_dps * (int32_t)dir;
    int32_t dv = dir * (int32_t)(((int64_t)a_mag * dt_fx) >> FX_SHIFT);
    
    // Apply acceleration, clamping to the cruise speed limit
    if (dir > 0) { // Moving positive
      if (velocity_dps < v_limit) { velocity_dps += dv; if (velocity_dps > v_limit) velocity_dps = v_limit; }
      else if (velocity_dps > v_limit) { velocity_dps -= dv; } // Decelerate if over-speed
    } else { // Moving negative
      if (velocity_dps > v_limit) { velocity_dps += dv; if (velocity_dps < v_limit) velocity_dps = v_limit; }
      else if (velocity_dps < v_limit) { velocity_dps -= dv; } // Decelerate if over-speed
    }
  }

  // --- 6. Stepping Logic (for Regular Motion) ---
  // This is the same 1-step-per-tick logic as in homing.
  position_deg += (int32_t)(((int64_t)velocity_dps * dt_fx) >> FX_SHIFT);
  long target_microsteps = degToSteps(position_deg);
  if (target_microsteps > microstep_counter) {
    setDir(HIGH);
    stepPulse();
    microstep_counter++; // "Reality" takes one step
  } else if (target_microsteps < microstep_counter) {
    setDir(LOW);
    stepPulse();
    microstep_counter--; // "Reality" takes one step
  }
}

// ------------------------------------------------------------------------
// Static and ISR Management
// ------------------------------------------------------------------------

/**
 * @brief [STATIC] The ISR calls this to update all active steppers.
 */
void StepperController::updateAll() {
  // Iterate through the linked list
  for (StepperController* p = g_list_head; p != nullptr; p = p->next_stepper) {
    p->update();
  }
}

/**
 * @brief Configures Timer1 for the ISR.
 */
void setupTimerInterrupt(uint32_t tick_hz) {
  cli(); // Disable interrupts

  TCCR1A = 0; // Clear Timer1 control registers
  TCCR1B = 0;

  // Find the best prescaler for the desired frequency
  const uint32_t cpu = F_CPU; // e.g., 16,000,000
  uint32_t prescalers[] = {1, 8, 64, 256, 1024};
  uint16_t chosenPrescaler = 1;
  uint16_t ocr = 0;

  for (uint8_t i = 0; i < 5; ++i) {
    uint32_t p = prescalers[i];
    // Calculate the timer TOP value (OCR1A)
    uint32_t top = (cpu / p) / tick_hz - 1;
    if (top <= 0xFFFFUL) { // Does it fit in a 16-bit register?
      chosenPrescaler = p;
      ocr = (uint16_t)top;
      break; // Found a working prescaler
    }
  }

  TCCR1B |= (1 << WGM12); // Enable CTC (Clear Timer on Compare) mode
  OCR1A = ocr;            // Set the TOP value

  // Set the prescaler bits in TCCR1B
  if (chosenPrescaler == 1) TCCR1B |= (1 << CS10);
  else if (chosenPrescaler == 8) TCCR1B |= (1 << CS11);
  else if (chosenPrescaler == 64) TCCR1B |= (1 << CS11) | (1 << CS10);
  else if (chosenPrescaler == 256) TCCR1B |= (1 << CS12);
  else if (chosenPrescaler == 1024) TCCR1B |= (1 << CS12) | (1 << CS10);

  TIMSK1 |= (1 << OCIE1A); // Enable the Timer1 Compare A interrupt
  sei(); // Re-enable global interrupts
}

/**
 * @brief The one and only ISR.
 * This is the "heartbeat" of the system.
 * With fixed-point math, the updateAll() function is extremely fast,
 * so we do not need to worry about it starving the Serial interrupt.
 */
ISR(TIMER1_COMPA_vect) {
  StepperController::updateAll(); // Update all registered stepper controllers
}