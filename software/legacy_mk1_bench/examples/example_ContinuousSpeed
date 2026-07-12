/**
 * @file main.cpp
 * @brief Example program demonstrating the continuous 'spin()' method.
 *
 * This application controls a single stepper motor, making it spin
 * continuously, pause, and reverse, all in a non-blocking way.
 *
 * It relies on the configurations in config.h to create a single
 * StepperController object.
 */

#include <Arduino.h>
#include "config.h"  // Pulls in all our hardware and config structs
#include "stepper.h" // Pulls in the StepperController class

// ========================================================================
// 1. CREATE CONTROLLER OBJECTS
// ========================================================================

// We only need one axis for this example.
// We'll use the "Pitch" axis hardware pins and configuration.
StepperController axisSpin(YAW_STEP_PIN, YAW_DIR_PIN, YAW_ENDSTOP_PIN, 
                           yawConfig, CONTROLLER_TICK_HZ);

// (We won't be using the Yaw axis in this example)

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
  // (This function is identical to the one in your previous main.cpp)
  StepperError error = axis.getError();
  if (error != NO_ERROR) {
    Serial.print("CRITICAL ERROR on axis ");
    Serial.print(axisName);
    Serial.print(": ");
    switch(error) {
      case TARGET_OUT_OF_BOUNDS: Serial.println("Target out of bounds. Move was clamped."); break;
      case HOMING_FAILED: Serial.println("Homing failed! Check endstop wiring/range."); break;
      default: Serial.println("Unknown error."); break;
    }
    axis.clearError();
  }
}


// ========================================================================
// 3. MAIN PROGRAM (SETUP & LOOP)
// ========================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) ; // Wait for serial connection
  Serial.println("Continuous Spin (spin()) example starting...");
  Serial.println("Motor will spin, stop, and reverse automatically.");

  // Initialize the stepper hardware pins
  axisSpin.begin();

  // Start the hardware timer ISR.
  // This "powers on" the motion system.
  setupTimerInterrupt(CONTROLLER_TICK_HZ);

  // NOTE: We are NOT homing. 
  // spin() does not require homing. Be aware that the motor's
  // 'position_deg' will be relative to its start-up position, 
  // which is defined by 'home_pos_deg' in the config (e.g., -30.0).

  Serial.println("Starting continuous spin (default speed, positive)...");
  // Call spin() with no arguments to use the default speed
  // (from config) and default direction (+1).
  axisSpin.spin();
}


void loop() {
  // This state machine demonstrates the non-blocking control of spin()
  
  static int state = 0;
  static unsigned long stateTimer = millis();

  // --- This part runs continuously in the background ---
  // It proves the main loop is not blocked by the spin() command.
  static unsigned long lastPrintMs = 0;
  if (millis() - lastPrintMs > 500) {
    Serial.print("Current (relative) position: ");
    Serial.println(axisSpin.getPositionDeg());
    lastPrintMs = millis();
  }
  // We can also check for errors
  checkAxisErrors(axisSpin, "SpinAxis");
  // --- End of continuous background tasks ---


  // --- State machine logic ---
  // This controls what the motor should be doing.
  switch (state) {
    case 0: // Spinning forward
      if (millis() - stateTimer > 5000) { // Spin for 5 seconds
        Serial.println("Stopping spin...");
        axisSpin.stop(); // Stop the motor
        stateTimer = millis();
        state = 1;
      }
      break;

    case 1: // Paused
      if (millis() - stateTimer > 2000) { // Pause for 2 seconds
        Serial.println("Spinning in reverse (200 dps)...");
        // Call spin() with custom speed (200.0) and direction (-1)
        axisSpin.spin(200.0f, -1); 
        stateTimer = millis();
        state = 2;
      }
      break;

    case 2: // Spinning reverse
      if (millis() - stateTimer > 5000) { // Spin for 5 seconds
        Serial.println("Stopping spin...");
        axisSpin.stop();
        stateTimer = millis();
        state = 3;
      }
      break;
    
    case 3: // Paused
       if (millis() - stateTimer > 1000) { // Pause for 1 seconds
        Serial.println("Spinning forward again (default speed)...");
        axisSpin.spin(); // Back to default
        stateTimer = millis();
        state = 0; // Loop back to state 0
      }
      break;
  }
}