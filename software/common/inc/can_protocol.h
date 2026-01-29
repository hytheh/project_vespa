/**
 * @file can_protocol.h
 * @brief Shared CAN Bus Communication Protocol for Project V.E.S.P.A.
 *
 * @section DESCRIPTION
 * This file defines the "Rosetta Stone" for the system. It contains the 
 * standard C structs and CAN ID definitions used by:
 * 1. The High-Level Commander (Jetson Orin Nano)
 * 2. The Motion Controllers (STM32 B-G431B-ESC1)
 * 3. The Coordinator/Safety Unit (ESP32-C6)
 * 4. The Capture Pulsing Unit (ESP32-S2)
 *
 * @section PROTOCOL_DESIGN
 * - Bus Speed: 1 Mbps (Standard CAN 2.0B)
 * - Endianness: Little Endian
 * - Safety: Watchdog Heartbeat required every 100ms.
 *
 * @section IMPLEMENTATION
 * - Coordinate System: Right-Hand Rule (X=Forward, Y=Left, Z=Up relative to Turret Base).
 * - Coordinates: Fixed Point. 1 LSB = 0.1mm.
 * Example: 1200mm -> 12000 (0x2EE0)
 * - Angles: Fixed Point. 1 LSB = 0.01 degrees.
 *
 * @note strictly C-compatible (POD types only) to ensure portability 
 * between C++ (Jetson) and C (Embedded Firmware).
 */

 /* =========================================================================
 * CAN BUS ID DEFINITIONS (11-bit Standard ID)
 * Priority: Lower Value = Higher Priority
 * ========================================================================= */

// 1. SAFETY CRITICAL (0x000 - 0x00F)
#define VESPA_CAN_ID_ESTOP          0x000   // Broadcast: Immediate Halt
#define VESPA_CAN_ID_SAFETY_HEARTBEAT 0x005 // Coordinator -> All (Watchdog)

// 2. HIGH SPEED CONTROL LOOP (0x010 - 0x02F)
#define VESPA_CAN_ID_TARGET_DATA    0x020   // Jetson -> Motion (XYZ + Confidence)

// 3. COMMAND & CONFIGURATION (0x030 - 0x05F)
#define VESPA_CAN_ID_FIRING_CMD     0x030   // Jetson -> Coordinator (Permission)
#define VESPA_CAN_ID_TRIGGER_CMD    0x040   // Jetson -> Trigger Node (Sync Setup)
#define VESPA_CAN_ID_FOCUS_CMD      0x045   // Coordinator -> Focus Stepper

// 4. TELEMETRY & STATUS (0x100 - 0x1FF)
#define VESPA_CAN_ID_PAN_STATUS     0x100   // STM32 Pan -> Coordinator/Jetson
#define VESPA_CAN_ID_TILT_STATUS    0x101   // STM32 Tilt -> Coordinator/Jetson
#define VESPA_CAN_ID_TRIGGER_STATUS 0x102   // Trigger Node -> Jetson
#define VESPA_CAN_ID_SYSTEM_LOG     0x110   // Debug strings (optional)

/* =========================================================================
 * PAYLOAD DEFINITIONS
 * ========================================================================= */

#pragma pack(push, 1) // Ensure no padding bytes

// -------------------------------------------------------------------------
// ID: VESPA_CAN_ID_TARGET_DATA (0x020)
// Usage: Sent by Vision Node every frame (~80Hz) to drive Motion
// -------------------------------------------------------------------------
typedef struct {
    int16_t x_mm_x10;      // X Coordinate (0.1mm) | Range: +/- 3.2m
    int16_t y_mm_x10;      // Y Coordinate (0.1mm) | Range: +/- 3.2m
    int16_t z_mm_x10;      // Z Coordinate (0.1mm) | Range: +/- 3.2m
    uint8_t confidence;    // 0-100% (VespAI Probability)
    uint8_t seq_counter;   // 0-255 (Packet continuity check)
} VespaTargetPacket;

// -------------------------------------------------------------------------
// ID: VESPA_CAN_ID_TRIGGER_CMD (0x040)
// Usage: Configure and Start/Stop the Camera Trigger Node
// -------------------------------------------------------------------------
typedef enum {
    TRIGGER_CMD_STOP = 0,
    TRIGGER_CMD_START = 1,
    TRIGGER_CMD_SET_FPS = 2
} TriggerCommandType;

typedef struct {
    uint8_t command;       // See TriggerCommandType
    uint8_t fps;           // Target FPS (e.g., 80)
    uint16_t pulse_width_us; // Exposure/Strobe time in microseconds
    uint8_t reserved[4];
} VespaTriggerCmdPacket;

// -------------------------------------------------------------------------
// ID: VESPA_CAN_ID_SAFETY_HEARTBEAT (0x005)
// Usage: Coordinator proves it's alive and monitoring safety
// -------------------------------------------------------------------------
typedef struct {
    uint8_t system_state;  // 0=Init, 1=Armed, 2=Firing, 255=Error
    uint8_t error_code;    // 0=None, 1=LidarObstructed, 2=Overheat...
    uint8_t laser_temp_c;  // Laser Diode Temperature
    uint8_t reserved[5];
} VespaSafetyPacket;

// -------------------------------------------------------------------------
// ID: VESPA_CAN_ID_TRIGGER_STATUS (0x102)
// Usage: Feedback from Trigger Node
// -------------------------------------------------------------------------
typedef struct {
    uint8_t is_running;    // 1=Pulsing, 0=Idle
    uint32_t frame_count;  // Total frames triggered
    uint8_t reserved[3];
} VespaTriggerStatusPacket;

#pragma pack(pop)

/* =========================================================================
 * HELPER MACROS
 * ========================================================================= */
#define MM_TO_INT16(mm) ((int16_t)((mm) * 10.0f))
#define INT16_TO_MM(val) ((float)(val) / 10.0f)

#endif // VESPA_CAN_PROTOCOL_H