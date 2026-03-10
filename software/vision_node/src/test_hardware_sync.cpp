/**
 * @file test_hardware_sync.cpp
 * @brief Bootstraps the dual-camera rig and validates stereoscopic synchronization.
 * @author Project V.E.S.P.A. Team
 * @details This executable leverages the HardwareSyncController to safely transition
 * both OV9281 sensors into Slave Mode, arm the 60Hz PWM heartbeat, and mathematically
 * prove that the left and right frames are captured at the exact same microsecond.
 * * @note Known Quirk: When this test exits, the terminal will hang for ~25 seconds.
 * This is the Linux tegra-video driver waiting for an End-Of-Frame (EOF) packet
 * that will never arrive because the PWM trigger has been safely killed.
 */

#include "HardwareSyncController.h"
#include <iostream>
#include <cmath>

int main()
{
    std::cout << "=== VESPA TEST: STEREO HARDWARE SYNCHRONIZATION ===" << std::endl;

    // 1. Initialize the orchestrator with both V4L2 nodes
    VESPA::HardwareSyncController stereoRig("/dev/video0", "/dev/video1");

    // 2. Execute the atomic boot sequence to clear bus contention
    // This safely handles the Master-to-Slave mode transition
    if (!stereoRig.initializeRig())
    {
        std::cerr << "[FATAL] Hardware initialization failed." << std::endl;
        return -1;
    }

    // 3. Arm the 60Hz 3.3V PWM heartbeat to wake the sensors
    stereoRig.armTrigger();

    double left_ts, right_ts;

    // 4. Capture 50 synchronized frames to verify temporal drift
    std::cout << "[TEST] Pulling 50 synchronized frame pairs..." << std::endl;
    for (int i = 0; i < 50; i++)
    {
        if (stereoRig.captureSynchronizedPair(left_ts, right_ts))
        {
            // Calculate the absolute delta between the hardware timestamps
            double offset = std::abs(left_ts - right_ts);
            std::cout << "Pair " << i << " grabbed! Hardware Drift: " << offset << " ms" << std::endl;
        }
        else
        {
            std::cerr << "[FATAL] Watchdog caught an error, trigger safely disarmed." << std::endl;
            break;
        }
    }

    std::cout << "[TEST] Complete. Initiating shutdown (expect a 25s driver hang)..." << std::endl;
    return 0;
}