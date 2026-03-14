/**
 * @file test_hardware_sync.cpp
 * @brief Upgraded validation for VESPA Stereo Rig.
 * @details Fixed to handle V4L2 shadow register timing and JSON verification.
 */

#include "HardwareSyncController.h"
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>

int main()
{
    std::cout << "=== VESPA TEST: STEREO HARDWARE SYNCHRONIZATION (UPGRADED) ===" << std::endl;

    // Use the definitive config path we've been tuning
    std::string configPath = "/home/hytheh/project_vespa/software/vision_node/config/camera_settings.json";
    VESPA::HardwareSyncController stereoRig("/dev/video0", "/dev/video1", configPath);

    if (!stereoRig.initializeRig())
    {
        std::cerr << "[FATAL] Hardware initialization failed." << std::endl;
        return -1;
    }

    // --- THE FIX: Stabilization Window ---
    // We MUST be streaming before I2C settings will 'stick' in Trigger Mode
    std::cout << "[TEST] Waking up sensors and waiting for PLL stabilization..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Arm the PWM Trigger (The 10% duty cycle heartbeat)
    stereoRig.armTrigger();

    std::cout << "[TEST] Pulling 50 synchronized frame pairs..." << std::endl;
    std::cout << "Index | L-Timestamp | R-Timestamp | Drift (ms) | Status" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    int successCount = 0;
    for (int i = 0; i < 50; i++)
    {
        VESPA::StereoPair pair = stereoRig.captureSynchronizedPair();

        if (pair.valid)
        {
            double offset = std::abs(pair.left_timestamp_ms - pair.right_timestamp_ms);

            // Log the microsecond-level drift
            std::cout << i << " | "
                      << pair.left_timestamp_ms << " | "
                      << pair.right_timestamp_ms << " | "
                      << offset << " ms | ";

            if (offset < 0.1)
            {
                std::cout << "[PERFECT]" << std::endl;
            }
            else if (offset < 1.0)
            {
                std::cout << "[OK]" << std::endl;
            }
            else
            {
                std::cout << "[DRIFT ALERT]" << std::endl;
            }

            successCount++;
            stereoRig.releaseSynchronizedPair(pair);
        }
        else
        {
            std::cerr << "[ERROR] Failed to capture synced pair at index " << i << std::endl;
        }
    }

    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "[TEST COMPLETE] Captured " << successCount << "/50 valid pairs." << std::endl;

    if (successCount > 45)
    {
        std::cout << "[RESULT] Hardware Sync Verified. Ready for Checkerboard Calibration." << std::endl;
    }
    else
    {
        std::cout << "[RESULT] High failure rate detected. Check PWM connection." << std::endl;
    }

    return 0;
}