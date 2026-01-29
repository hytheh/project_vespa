/**
 * @file test_protocol.cpp
 * @brief Validation script for CAN Protocol Alignment
 * compile with: g++ test_protocol.cpp -o test_protocol
 */

#include <iostream>
#include <cstring>
#include <iomanip>
#include "../software/common/inc/can_protocol.h"

void print_result(const char* test_name, bool passed) {
    std::cout << std::left << std::setw(40) << test_name 
              << (passed ? "[PASS]" : "[FAIL]") << std::endl;
}

int main() {
    std::cout << "=== V.E.S.P.A. Protocol Validation ===" << std::endl;

    // 1. Check Struct Sizes
    print_result("VespaTargetPacket Size == 8", sizeof(VespaTargetPacket) == 8);
    print_result("VespaTriggerCmdPacket Size == 8", sizeof(VespaTriggerCmdPacket) == 8);
    print_result("VespaSafetyPacket Size == 8", sizeof(VespaSafetyPacket) == 8);

    // 2. Check Coordinate Packing logic
    float input_z = 1200.5f; // 1200.5 mm
    int16_t packed_z = MM_TO_INT16(input_z);
    
    // Check if scaling works (1200.5 * 10 = 12005)
    print_result("Scaling Logic (1200.5mm -> 12005)", packed_z == 12005);

    // Check if it fits in buffer
    VespaTargetPacket packet;
    packet.x_mm_x10 = MM_TO_INT16(100.0f);
    packet.y_mm_x10 = MM_TO_INT16(-50.5f);
    packet.z_mm_x10 = packed_z;
    packet.confidence = 98;
    packet.seq_counter = 1;

    // Simulate CAN transmission (memcpy to byte array)
    uint8_t frame_data[8];
    std::memcpy(frame_data, &packet, 8);

    // Simulate Reception
    VespaTargetPacket received;
    std::memcpy(&received, frame_data, 8);

    float decoded_z = INT16_TO_MM(received.z_mm_x10);
    print_result("Pack/Unpack Integrity", (decoded_z == 1200.5f));

    return 0;
}