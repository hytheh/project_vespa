/**
 * @file can_protocol.h
 * @author Lead Software Comm Developer
 * @brief Common CAN message definitions and data structures for Project VESPA.
 * @date 2026-03-16
 */

#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** * @brief Scale factor for Pan/Tilt angles in radians. 
 * Multiply radians by this factor before transmitting.
 * Divide received int16 by 10000.0f to get the float value in radians. 
 */
#define ANGLE_SCALE_FACTOR 10000.0f

/** @brief Macro to instruct the compiler not to pad the structures */
#define CAN_PACKED __attribute__((packed))

/**
 * @defgroup CAN_IDS CAN Message Identifiers
 * Standard 11-bit CAN IDs. Lower ID = Higher Priority.
 * @{
 */
#define CAN_ID_FIRE_CMD     0x010   /**< Fire command (Highest Priority) */
#define CAN_ID_TARGET_VEC   0x020   /**< 3D Target Vector (Pan, Tilt, Depth) */
#define CAN_ID_POS_MOTION   0x030   /**< Absolute Position from Motion Node */
#define CAN_ID_POS_COORD    0x040   /**< Absolute Position from Coordinator Node */
#define CAN_ID_HB_VISION    0x100   /**< Vision Node Heartbeat */
#define CAN_ID_HB_MOTION    0x110   /**< Motion Node Heartbeat */
#define CAN_ID_HB_COORD     0x120   /**< Coordinator Node Heartbeat */
#define CAN_ID_DBG_MOTION   0x200   /**< Motion Node Debug States */
#define CAN_ID_DBG_COORD    0x210   /**< Coordinator Node Debug States */
/** @} */

/**
 * @brief Payload structure for TARGET_VEC (CAN_ID_TARGET_VEC)
 * Publisher: vision_node | Subscriber: motion_node
 */
typedef struct {
    int16_t pan_angle;    /**< Target Pan angle in radians. Scaled by 10000 */
    int16_t tilt_angle;   /**< Target Tilt angle in radians. Scaled by 10000 */
    uint16_t depth_mm;    /**< Absolute distance to target in millimeters */
    uint8_t is_locked;    /**< 1 if target is locked in aimsight, 0 otherwise */
} CAN_PACKED TargetPayload_t;

/**
 * @brief Payload structure for POS_MOTION (CAN_ID_POS_MOTION)
 * Publisher: motion_node | Subscriber: vision_node
 */
typedef struct {
    int16_t current_pan;  /**< Current Pan angle in radians. Scaled by 10000 */
    int16_t current_tilt; /**< Current Tilt angle in radians. Scaled by 10000 */
} CAN_PACKED MotionPosPayload_t;

/**
 * @brief Payload structure for POS_COORD (CAN_ID_POS_COORD)
 * Publisher: coordinator_node | Subscriber: vision_node
 */
typedef struct {
    int32_t position_mm;  /**< Current collimator linear position in millimeters */
} CAN_PACKED CoordPosPayload_t;

/**
 * @brief Payload structure for Heartbeat messages
 * Publisher: All nodes | Subscriber: coordinator_node (Watchdog)
 */
typedef struct {
    uint8_t system_state; /**< 0x00: OK, 0x01: WARNING, 0x02: FAULT/ERROR */
} CAN_PACKED HeartbeatPayload_t;

/**
 * @brief Payload structure for FIRE_CMD (CAN_ID_FIRE_CMD)
 * Publisher: vision_node | Subscriber: coordinator_node
 */
typedef struct {
    uint8_t fire_order;   /**< 0x01 to activate laser, 0x00 to hold */
} CAN_PACKED FirePayload_t;

#ifdef __cplusplus
}
#endif

#endif // CAN_PROTOCOL_H