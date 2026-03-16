/**
 * @file motion_can.h
 * @author Lead Software Comm Developer
 * @brief STM32G431 FDCAN driver wrapper for Project VESPA.
 * @date 2026-03-16
 */

#ifndef MOTION_CAN_H
#define MOTION_CAN_H

#include <Arduino.h>
#include "stm32g4xx_hal.h"
#include "can_utils.h"

class MotionCan {
public:
    /**
     * @brief Construct a new Motion Can object
     * @param rxPin Arduino pin name for CAN RX (Default: PA11)
     * @param txPin Arduino pin name for CAN TX (Default: PA12)
     */
    MotionCan(uint32_t rxPin = PA11, uint32_t txPin = PA12);

    /**
     * @brief Initializes the FDCAN peripheral in Classic CAN 2.0 mode at 500kbps.
     * @return true if initialization and filter setup succeeds, false otherwise.
     */
    bool begin();

    /**
     * @brief Transmits a common VESPA CAN frame.
     * @param frame Pointer to the frame to transmit.
     * @return true if pushed to the TX FIFO successfully.
     */
    bool sendFrame(const CAN_Frame_t* frame);

    /**
     * @brief Checks the RX FIFO and pulls a received frame if available.
     * @param frame Pointer to populate with received data.
     * @return true if a frame was successfully read.
     */
    bool readFrame(CAN_Frame_t* frame);

private:
    uint32_t _rxPin;
    uint32_t _txPin;
    FDCAN_HandleTypeDef _hfdcan;

    void setupHardwarePins();
    bool configureFilters();
};

#endif // MOTION_CAN_H