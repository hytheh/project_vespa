#include <Arduino.h>
#include "motion_can.h"

MotionCan canBus(PA11, PA12);
CAN_Frame_t rxFrame;
CAN_Frame_t txFrame;

void setup() {
    canBus.begin();
}

void loop() {
    // 1. Read targeting vectors
    if (canBus.readFrame(&rxFrame)) {
        TargetPayload_t target;
        if (CAN_Decode_TargetVec(&rxFrame, &target)) {
            // Apply new pan/tilt to motors...
        }
    }

    // 2. Transmit error metrics
    TrackingErrorPayload_t errPayload = { .error_mm = 15 }; // example error
    if (CAN_Encode_TrackingError(&txFrame, CAN_ID_ERR_MOTION, &errPayload)) {
        canBus.sendFrame(&txFrame);
    }
    
    delay(16); // roughly 60Hz loop
}