/**
 * @file CAN_integration_example.cpp
 * @brief Minimal reference for the motion_node CAN loop, using the CURRENT
 *        protocol defined in software/common/can_protocol.h + can_utils.h.
 *
 * This is documentation only (files under Examples/ are not part of the
 * PlatformIO build). See src/main.cpp for the integrated firmware.
 */
#include <Arduino.h>
#include "motion_can.h"

MotionCan canBus(PA11, PA12);
CAN_Frame_t rxFrame;
CAN_Frame_t txFrame;

void setup() {
    canBus.begin();
}

void loop() {
    // 1. Receive the 3D target vector published by the vision node.
    if (canBus.readFrame(&rxFrame)) {
        TargetPayload_t target;
        if (CAN_Decode_TargetVec(&rxFrame, &target)) {
            float pan_rad  = target.pan_angle  / ANGLE_SCALE_FACTOR;
            float tilt_rad = target.tilt_angle / ANGLE_SCALE_FACTOR;
            // Command the FOC controllers with pan_rad / tilt_rad while
            // target.is_locked is set (see src/main.cpp).
            (void)pan_rad;
            (void)tilt_rad;
        }
    }

    // 2. Publish our current absolute position back to the vision node.
    MotionPosPayload_t posPayload;
    posPayload.current_pan  = 0;   // (int16_t)(shaft_angle * ANGLE_SCALE_FACTOR)
    posPayload.current_tilt = 0;
    if (CAN_Encode_MotionPos(&txFrame, &posPayload)) {
        canBus.sendFrame(&txFrame);
    }

    // 3. Emit a 1 Hz heartbeat so the coordinator's watchdog sees us alive.
    HeartbeatPayload_t hb = { .system_state = 0x00 };
    if (CAN_Encode_Heartbeat(&txFrame, CAN_ID_HB_MOTION, &hb)) {
        canBus.sendFrame(&txFrame);
    }

    delay(16); // roughly 60 Hz loop
}
