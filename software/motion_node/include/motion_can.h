/**
 * @file motion_can.h
 * @author Lead Software Comm Developer / Motion Node Lead
 * @brief STM32G431 FDCAN driver wrapper for Project VESPA.
 */

#ifndef MOTION_CAN_H
#define MOTION_CAN_H

#include <Arduino.h>
#include "stm32g4xx_hal.h"
#include "can_utils.h"

class MotionCan {
public:
    MotionCan(uint32_t rxPin = PA11, uint32_t txPin = PA12);
    bool begin();
    bool sendFrame(const CAN_Frame_t* frame);
    bool readFrame(CAN_Frame_t* frame);

private:
    uint32_t _rxPin;
    uint32_t _txPin;
    FDCAN_HandleTypeDef _hfdcan;

    void setupHardwarePins();
    bool configureFilters();
};

#endif // MOTION_CAN_H