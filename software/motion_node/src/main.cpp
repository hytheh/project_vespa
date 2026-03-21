#include <Arduino.h>
#include <SPI.h>
#include <SimpleFOC.h>
#include "pinout.h" 

// --- CAN BUS INCLUDES ---
#include "motion_can.h"
#include "can_utils.h"

// --- GM5208-12 GIMBAL PARAMETERS ---
const int   POLE_PAIRS = 7; 
const float RS_OHMS    = 7.476f;
const float LS_HENRYS  = 0.00449f;
const float KV_RATING  = 21.4f;

// --- CALIBRATION VALUES ---
const float PAN_ZERO_ANGLE  = 0.2684f; 
const Direction PAN_DIR     = Direction::CW; 
const float TILT_ZERO_ANGLE = 0.7543f; 
const Direction TILT_DIR    = Direction::CW;

volatile bool estop_triggered = false;

// --- HARDWARE OBJECTS ---
MagneticSensorSPI encoderPan  = MagneticSensorSPI(AS5048_SPI, ENC1_CS);
MagneticSensorSPI encoderTilt = MagneticSensorSPI(AS5048_SPI, ENC2_CS);

BLDCDriver3PWM driverPan  = BLDCDriver3PWM(M1_INU, M1_INV, M1_INW, M1_EN_FAULT);
BLDCDriver3PWM driverTilt = BLDCDriver3PWM(M2_INU, M2_INV, M2_INW, M2_EN_FAULT);

BLDCMotor motorPan  = BLDCMotor(POLE_PAIRS, RS_OHMS, KV_RATING, LS_HENRYS);
BLDCMotor motorTilt = BLDCMotor(POLE_PAIRS, RS_OHMS, KV_RATING, LS_HENRYS);

// Initialize CAN Bus with our correct pins from pinout.h
MotionCan canBus(CAN_RX, CAN_TX);
CAN_Frame_t rxFrame;

// --- LIMITS & OFFSETS (IN RADIANS) ---
const float PAN_HARDCODED_OFFSET  = 0.00f; 
const float TILT_HARDCODED_OFFSET = 0.00f; 

const float PAN_MIN_LIMIT  = -0.52f; // -30 deg
const float PAN_MAX_LIMIT  =  0.52f; // +30 deg
const float TILT_MIN_LIMIT = -0.26f; // -15 deg
const float TILT_MAX_LIMIT =  0.78f; // +45 deg

float pan_base_target  = 0.0f;
float tilt_base_target = 0.0f;

// --- FILTERS ---
LowPassFilter filter_pot_pan  = LowPassFilter(0.05f);
LowPassFilter filter_pot_tilt = LowPassFilter(0.05f);

// The "Shock Absorbers" for network jitter
LowPassFilter trajectory_pan  = LowPassFilter(1.5f);
LowPassFilter trajectory_tilt = LowPassFilter(1.5f);

// --- SOFT E-STOP ---
void emergencyStopISR() {
  estop_triggered = true;
  digitalWrite(M1_EN_FAULT, LOW); 
  digitalWrite(M2_EN_FAULT, LOW); 
}

void setup() {
  Serial.begin(115200);
  
  pinMode(USER_BUTTON, INPUT); 
  attachInterrupt(digitalPinToInterrupt(USER_BUTTON), emergencyStopISR, RISING);
  delay(2000); 
  
  pinMode(ENC1_CS, OUTPUT);  digitalWrite(ENC1_CS, HIGH);
  pinMode(ENC2_CS, OUTPUT);  digitalWrite(ENC2_CS, HIGH);
  pinMode(SPI1_NSS, INPUT_PULLUP); 
  pinMode(POT_PAN, INPUT);
  pinMode(POT_TILT, INPUT);

  SPI.begin();
  encoderPan.init(&SPI);
  encoderTilt.init(&SPI);
  motorPan.linkSensor(&encoderPan);
  motorTilt.linkSensor(&encoderTilt);

  driverPan.voltage_power_supply = 24.0f;
  driverPan.init();
  motorPan.linkDriver(&driverPan);

  driverTilt.voltage_power_supply = 24.0f;
  driverTilt.init();
  motorTilt.linkDriver(&driverTilt);

  motorPan.controller  = MotionControlType::angle;
  motorTilt.controller = MotionControlType::angle;

  motorPan.current_limit  = 1.0f; 
  motorPan.velocity_limit = 10.0f; 

  motorTilt.current_limit  = 1.0f; 
  motorTilt.velocity_limit = 10.0f;

  // --- PID TUNING ---
  motorPan.P_angle.P = 4.0f;
  motorPan.P_angle.I = 0.5f;  
  motorPan.P_angle.D = 0.0f;
  motorPan.LPF_velocity.Tf = 0.005f; 

  motorTilt.P_angle.P = 6.0f; 
  motorTilt.P_angle.I = 0.6f; 
  motorTilt.P_angle.D = 0.00f; 
  motorTilt.LPF_velocity.Tf = 0.005f;

  // Push the 0/2PI rollover safely to the back
  motorPan.sensor_offset  = 4.7500f; 
  motorTilt.sensor_offset = 4.8179f;

  motorPan.init();
  motorTilt.init();

  Serial.println("\n--- Initializing FOC silently ---");
  motorPan.zero_electric_angle = PAN_ZERO_ANGLE;
  motorPan.sensor_direction    = PAN_DIR;
  motorPan.initFOC(); 

  motorTilt.zero_electric_angle = TILT_ZERO_ANGLE;
  motorTilt.sensor_direction    = TILT_DIR;
  motorTilt.initFOC(); 

  // --- INITIALIZE CAN BUS ---
  if (canBus.begin()) {
    Serial.println("=== CAN BUS INITIALIZED SUCCESSFULLY ===");
  } else {
    Serial.println("!!! CAN BUS INIT FAILED !!!");
  }

  Serial.println("=== SYSTEM READY. SILENT STARTUP SUCCESS ===");
}

void loop() {
  // 0. EMERGENCY STOP TRAP
  if (estop_triggered) {
    motorPan.disable();
    motorTilt.disable();
    
    // Broadcast ERR state before locking up
    HeartbeatPayload_t hbPayload;
    hbPayload.system_state = 0x02; // 0x02 = ERR/FAULT
    CAN_Frame_t txFrame;
    if (CAN_Encode_Heartbeat(&txFrame, CAN_ID_HB_MOTION, &hbPayload)) {
        canBus.sendFrame(&txFrame);
    }
    
    Serial.println("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("!!!  EMERGENCY STOP TRIGGERED        !!!");
    Serial.println("!!!  MOTORS DISABLED                 !!!");
    Serial.println("!!!  PRESS BLACK RESET BTN TO REBOOT !!!");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    
    Serial.flush();
    while(true) { delay(1000); }
  }

  // 1. POLL CAN BUS FOR TARGET VECTORS
  if (canBus.readFrame(&rxFrame)) {
    TargetPayload_t target;
    if (CAN_Decode_TargetVec(&rxFrame, &target)) {
        // Yes, we are targeting here! The CAN payload directly overwrites the base target.
        pan_base_target  = target.pan_angle / ANGLE_SCALE_FACTOR;
        tilt_base_target = target.tilt_angle / ANGLE_SCALE_FACTOR;
    }
  }

  // 2. Read and filter potentiometers (Manual override mix-in)
  float raw_pot_pan  = analogRead(POT_PAN);
  float raw_pot_tilt = analogRead(POT_TILT);

  float offset_pan  = ((raw_pot_pan / 1023.0f) * 0.5236f) - 0.2618f;
  float offset_tilt = ((raw_pot_tilt / 1023.0f) * 0.5236f) - 0.2618f;

  float smooth_offset_pan  = filter_pot_pan(offset_pan);
  float smooth_offset_tilt = filter_pot_tilt(offset_tilt);

  // 3. Calculate the raw requested targets
  float requested_pan  = pan_base_target + PAN_HARDCODED_OFFSET + smooth_offset_pan;
  float requested_tilt = tilt_base_target + TILT_HARDCODED_OFFSET + smooth_offset_tilt;

  // 4. Brutally clamp the targets to safe physical limits
  float clamped_pan  = constrain(requested_pan, PAN_MIN_LIMIT, PAN_MAX_LIMIT);
  float clamped_tilt = constrain(requested_tilt, TILT_MIN_LIMIT, TILT_MAX_LIMIT);

  // 5. Pass the clamped target directly to the motor via the Trajectory Filter
  motorPan.target  = trajectory_pan(clamped_pan);
  motorTilt.target = trajectory_tilt(clamped_tilt);

  // 6. Execute FOC
  motorPan.loopFOC();
  motorPan.move();
  motorTilt.loopFOC();
  motorTilt.move();

  // 7. TRANSMIT POS_MOTION (Running at 20Hz / every 50ms)
  static unsigned long last_pos_tx = 0;
  if (millis() - last_pos_tx > 50) {
      last_pos_tx = millis();

      MotionPosPayload_t posPayload;
      posPayload.current_pan  = (int16_t)(motorPan.shaft_angle * ANGLE_SCALE_FACTOR);
      posPayload.current_tilt = (int16_t)(motorTilt.shaft_angle * ANGLE_SCALE_FACTOR);

      CAN_Frame_t txFrame;
      if (CAN_Encode_MotionPos(&txFrame, &posPayload)) {
          canBus.sendFrame(&txFrame);
      }
  }

  // 8. TRANSMIT HB_MOTION (Running at 1Hz / every 1000ms)
  static unsigned long last_hb_tx = 0;
  if (millis() - last_hb_tx > 1000) {
      last_hb_tx = millis();

      HeartbeatPayload_t hbPayload;
      hbPayload.system_state = 0x00; // 0x00 = OK

      CAN_Frame_t txFrame;
      if (CAN_Encode_Heartbeat(&txFrame, CAN_ID_HB_MOTION, &hbPayload)) {
          canBus.sendFrame(&txFrame);
      }
  }
}