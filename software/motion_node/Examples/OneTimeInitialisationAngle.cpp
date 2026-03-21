#include <Arduino.h>
#include <SPI.h>
#include <SimpleFOC.h>

const int   POLE_PAIRS = 7; 
const float RS_OHMS    = 7.476f;
const float KV_RATING  = 21.4f;
const float LS_HENRYS  = 0.00449f;

#define CS_PAN   PB6
#define CS_TILT  PB7

MagneticSensorSPI encoderPan  = MagneticSensorSPI(AS5048_SPI, CS_PAN);
MagneticSensorSPI encoderTilt = MagneticSensorSPI(AS5048_SPI, CS_TILT);

BLDCDriver3PWM driverPan  = BLDCDriver3PWM(PA8, PA9, PA10, PA11);
BLDCDriver3PWM driverTilt = BLDCDriver3PWM(PC6, PC7, PC8, PB12);

BLDCMotor motorPan  = BLDCMotor(POLE_PAIRS, RS_OHMS, KV_RATING, LS_HENRYS);
BLDCMotor motorTilt = BLDCMotor(POLE_PAIRS, RS_OHMS, KV_RATING, LS_HENRYS);

void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.println("\n--- RUNNING ONE-TIME CALIBRATION ---");

  pinMode(CS_PAN, OUTPUT);  digitalWrite(CS_PAN, HIGH);
  pinMode(CS_TILT, OUTPUT); digitalWrite(CS_TILT, HIGH);
  pinMode(PA4, INPUT_PULLUP);

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

  // We need enough voltage to confidently find the magnetic zero
  motorPan.voltage_sensor_align  = 12.0f;
  motorTilt.voltage_sensor_align = 12.0f;

  motorPan.init();
  motorPan.initFOC(); // This will jolt

  motorTilt.init();
  motorTilt.initFOC(); // This will jolt

  Serial.println("\n=============================================");
  Serial.println("CALIBRATION COMPLETE. COPY THESE VALUES:");
  Serial.println("=============================================");
  Serial.print("PAN_ZERO_ANGLE: "); Serial.println(motorPan.zero_electric_angle, 4);
  Serial.print("PAN_DIRECTION:  "); Serial.println(motorPan.sensor_direction == Direction::CW ? "Direction::CW" : "Direction::CCW");
  Serial.println("---------------------------------------------");
  Serial.print("TILT_ZERO_ANGLE: "); Serial.println(motorTilt.zero_electric_angle, 4);
  Serial.print("TILT_DIRECTION:  "); Serial.println(motorTilt.sensor_direction == Direction::CW ? "Direction::CW" : "Direction::CCW");
  Serial.println("=============================================\n");
}

void loop() {
  // Do nothing
}