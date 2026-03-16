/**
 * @file can_utils.h
 * @author Lead Software Comm Developer
 * @brief Hardware-agnostic CAN serialization/deserialization utilities.
 * @date 2026-03-16
 */

#ifndef CAN_UTILS_H
#define CAN_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "can_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/**
 * @brief Generic CAN Frame structure used across all VESPA nodes.
 */
typedef struct {
    uint32_t id;        /**< Standard 11-bit CAN ID */
    uint8_t dlc;        /**< Data Length Code (0-8 bytes) */
    uint8_t data[8];    /**< Frame payload */
} CAN_Frame_t;

/* ========================================================================== */
/* Serialization (Encode: Struct -> Frame)                                    */
/* ========================================================================== */

bool CAN_Encode_TargetVec(CAN_Frame_t* frame, const TargetPayload_t* payload);
bool CAN_Encode_MotionPos(CAN_Frame_t* frame, const MotionPosPayload_t* payload);
bool CAN_Encode_CoordPos(CAN_Frame_t* frame, const CoordPosPayload_t* payload);
bool CAN_Encode_Heartbeat(CAN_Frame_t* frame, uint32_t msg_id, const HeartbeatPayload_t* payload);
bool CAN_Encode_FireCmd(CAN_Frame_t* frame, const FirePayload_t* payload);

/* ========================================================================== */
/* Deserialization (Decode: Frame -> Struct)                                  */
/* ========================================================================== */

bool CAN_Decode_TargetVec(const CAN_Frame_t* frame, TargetPayload_t* payload);
bool CAN_Decode_MotionPos(const CAN_Frame_t* frame, MotionPosPayload_t* payload);
bool CAN_Decode_CoordPos(const CAN_Frame_t* frame, CoordPosPayload_t* payload);
bool CAN_Decode_Heartbeat(const CAN_Frame_t* frame, HeartbeatPayload_t* payload);
bool CAN_Decode_FireCmd(const CAN_Frame_t* frame, FirePayload_t* payload);

#ifdef __cplusplus
}
#endif

#endif // CAN_UTILS_H