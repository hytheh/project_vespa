/**
 * @file can_utils.c
 * @author Lead Software Comm Developer
 * @brief Implementation of hardware-agnostic CAN SerDes.
 * @date 2026-03-16
 */

#include "can_utils.h"

/* ========================================================================== */
/* Serialization Logic                                                        */
/* ========================================================================== */

bool CAN_Encode_TargetVec(CAN_Frame_t* frame, const TargetPayload_t* payload) {
    if (!frame || !payload) return false;
    frame->id = CAN_ID_TARGET_VEC;
    frame->dlc = sizeof(TargetPayload_t);
    memcpy(frame->data, payload, frame->dlc);
    return true;
}

bool CAN_Encode_MotionPos(CAN_Frame_t* frame, const MotionPosPayload_t* payload) {
    if (!frame || !payload) return false;
    frame->id = CAN_ID_POS_MOTION;
    frame->dlc = sizeof(MotionPosPayload_t);
    memcpy(frame->data, payload, frame->dlc);
    return true;
}

bool CAN_Encode_CoordPos(CAN_Frame_t* frame, const CoordPosPayload_t* payload) {
    if (!frame || !payload) return false;
    frame->id = CAN_ID_POS_COORD;
    frame->dlc = sizeof(CoordPosPayload_t);
    memcpy(frame->data, payload, frame->dlc);
    return true;
}

bool CAN_Encode_Heartbeat(CAN_Frame_t* frame, uint32_t msg_id, const HeartbeatPayload_t* payload) {
    if (!frame || !payload) return false;
    if (msg_id != CAN_ID_HB_VISION && msg_id != CAN_ID_HB_MOTION && msg_id != CAN_ID_HB_COORD) return false;
    
    frame->id = msg_id;
    frame->dlc = sizeof(HeartbeatPayload_t);
    memcpy(frame->data, payload, frame->dlc);
    return true;
}

bool CAN_Encode_FireCmd(CAN_Frame_t* frame, const FirePayload_t* payload) {
    if (!frame || !payload) return false;
    frame->id = CAN_ID_FIRE_CMD;
    frame->dlc = sizeof(FirePayload_t);
    memcpy(frame->data, payload, frame->dlc);
    return true;
}

/* ========================================================================== */
/* Deserialization Logic                                                      */
/* ========================================================================== */

bool CAN_Decode_TargetVec(const CAN_Frame_t* frame, TargetPayload_t* payload) {
    if (!frame || !payload || frame->id != CAN_ID_TARGET_VEC || frame->dlc != sizeof(TargetPayload_t)) {
        return false;
    }
    memcpy(payload, frame->data, frame->dlc);
    return true;
}

bool CAN_Decode_MotionPos(const CAN_Frame_t* frame, MotionPosPayload_t* payload) {
    if (!frame || !payload || frame->id != CAN_ID_POS_MOTION || frame->dlc != sizeof(MotionPosPayload_t)) {
        return false;
    }
    memcpy(payload, frame->data, frame->dlc);
    return true;
}

bool CAN_Decode_CoordPos(const CAN_Frame_t* frame, CoordPosPayload_t* payload) {
    if (!frame || !payload || frame->id != CAN_ID_POS_COORD || frame->dlc != sizeof(CoordPosPayload_t)) {
        return false;
    }
    memcpy(payload, frame->data, frame->dlc);
    return true;
}

bool CAN_Decode_Heartbeat(const CAN_Frame_t* frame, HeartbeatPayload_t* payload) {
    if (!frame || !payload || frame->dlc != sizeof(HeartbeatPayload_t)) return false;
    if (frame->id != CAN_ID_HB_VISION && frame->id != CAN_ID_HB_MOTION && frame->id != CAN_ID_HB_COORD) return false;
    
    memcpy(payload, frame->data, frame->dlc);
    return true;
}

bool CAN_Decode_FireCmd(const CAN_Frame_t* frame, FirePayload_t* payload) {
    if (!frame || !payload || frame->id != CAN_ID_FIRE_CMD || frame->dlc != sizeof(FirePayload_t)) {
        return false;
    }
    memcpy(payload, frame->data, frame->dlc);
    return true;
}