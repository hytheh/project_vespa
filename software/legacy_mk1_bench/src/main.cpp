/**
 * @file main.cpp
 * @brief Main application file for the 2-axis robot.
 *
 * This file is the "user" of the StepperController library.
 * It uses the configurations from config.h to create the stepper
 * objects and then implements the robot's main logic (setup and loop).
 */

#include <Arduino.h>
#include "config.h"  // Pulls in all our hardware and config structs
#include "stepper.h" // Pulls in the StepperController class

// ========================================================================
// 1. CREATE CONTROLLER OBJECTS
// ========================================================================
// Create the global instances for each axis.
// We pass in the pins and config struct from config.h.
// The constructor automatically registers them with the ISR.
StepperController axisPitch(PITCH_STEP_PIN, PITCH_DIR_PIN, PITCH_ENDSTOP_PIN, 
                          pitchConfig, CONTROLLER_TICK_HZ);
                          
StepperController axisYaw(YAW_STEP_PIN, YAW_DIR_PIN, YAW_ENDSTOP_PIN, 
                        yawConfig, CONTROLLER_TICK_HZ);

// ========================================================================
// 2. HELPER FUNCTIONS
// ========================================================================

/**
 * @brief Helper function to check an axis for errors and print them.
 * This is polled from the main loop.
 * @param axis The StepperController object to check.
 * @param axisName A string name for printing.
 */
void checkAxisErrors(StepperController &axis, const char* axisName) {
  StepperError error = axis.getError();
  
  if (error != NO_ERROR) {
    Serial.print("CRITICAL ERROR on axis ");
    Serial.print(axisName);
    Serial.print(": ");

    switch(error) {
      case TARGET_OUT_OF_BOUNDS:
        Serial.println("Target out of bounds. Move was clamped.");
        break;
      case HOMING_FAILED:
        Serial.println("Homing failed! Check endstop wiring and travel range.");
        // We could halt the program here if needed
        // while(1) { delay(100); }
        break;
      default:
        Serial.println("Unknown error.");
        break;
    }
    
    axis.clearError(); // Acknowledge the error so it doesn't repeat
  }
}


// ========================================================================
// 3. MAIN PROGRAM (SETUP & LOOP)
// ========================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) ; // Wait for serial connection
  Serial.println("Scalable Stepper Controller example starting...");

  // Initialize the stepper hardware pins
  axisPitch.begin();
  axisYaw.begin();

  // Start the hardware timer ISR.
  // This "powers on" the motion system.
  setupTimerInterrupt(CONTROLLER_TICK_HZ);

  // --- Homing Sequence ---
  // We perform homing in setup() to ensure the robot knows its
  // position before the main loop begins. This is a "blocking"
  // operation (it waits here) but the ISRs are still running.
  
  Serial.println("Starting homing pitch stepper...");
  axisPitch.startHoming(); // This is non-blocking

  // Wait for the homing to finish
  while (!axisPitch.isHomed()) {
    checkAxisErrors(axisPitch, "Pitch"); // Check for failures
    delay(10); // Give the main CPU a tiny break
  }
  Serial.print("Homing Pitch complete. Position: ");
  Serial.println(axisPitch.getPositionDeg());

  Serial.println("Starting homing yaw stepper...");
  axisYaw.startHoming();

  while (!axisYaw.isHomed()) {
    checkAxisErrors(axisYaw, "Yaw");
    delay(10);
  }
  Serial.print("Homing Yaw complete. Position: ");
  Serial.println(axisYaw.getPositionDeg());

}


void loop() {
  // --- 1. Check for Errors ---
  // This should be done at the start of every loop.
  checkAxisErrors(axisPitch, "Pitch");
  checkAxisErrors(axisYaw, "Yaw");

  // --- 2. Main Robot Logic (Non-blocking) ---
  // This is a simple state machine that moves the motor back and forth.
  static bool goingTo = true;
  static unsigned long lastChangeMs = 0;
  const unsigned long pauseMs = 2000; // 2.0 sec pause

  // We only schedule a new move IF the motor is not busy
  if (!axisPitch.isMoving() && !axisYaw.isMoving()) {
    // Wait for a small pause before starting the next move
    if ((!goingTo) && (millis() - lastChangeMs < pauseMs)) {
      return; // Not time to move yet
    }

    // It's time! Schedule the next move.
    if (goingTo) {
      Serial.println("Loop: move to 150;120°");
      axisPitch.moveToDeg(150.0f); // Uses default speed/accel
      axisYaw.moveToDeg(120.0f); // Uses default speed/accel
    } else {
      Serial.println("Loop: move to 0;0°");
      axisPitch.moveToDeg(10.0f); // Uses default speed/accel
      axisYaw.moveToDeg(10.0f); // Uses default speed/accel
    }
    
    goingTo = !goingTo; // Toggle the state
    lastChangeMs = millis(); // Reset the timer
  }

  // --- 3. Other Tasks ---
  // You could read sensors, check for serial commands, etc., here.
  // As long as nothing "blocks" (like a long delay()), the
  // motor motion will be perfectly smooth.
}