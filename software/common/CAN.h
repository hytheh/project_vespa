/**
 * @file CAN.h
 * @brief Common CAN Bus Protocol Definitions for all nodes.
 */

#ifndef CAN_H
#define CAN_H

#include <stdint.h>

// CAN IDs
#define CAN_ID_VISION_TARGET    0x100
#define CAN_ID_MOTION_STATUS    0x200
#define CAN_ID_SYSTEM_STATE     0x300
#define CAN_ID_LASER_CMD        0x400

typedef struct {
    int16_t azimuth;   // 0.01 degree per bit
    int16_t elevation; // 0.01 degree per bit
    uint16_t distance; // mm
    uint8_t confidence;
    uint8_t flags;
} TargetData_t;

#endif // CAN_H
