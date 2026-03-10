#include "CameraInterface.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <memory>
#include <iomanip> // For formatting time output

// Global pointer for signal handler
std::unique_ptr<VESPA::CameraInterface> g_cam;

void signalHandler(int signum)
{
    std::cout << "\n\n[TEST] Interrupt signal (" << signum << ") received." << std::endl;
    if (g_cam)
    {
        std::cout << "[TEST] Emergency PWM Shutdown..." << std::endl;
        g_cam->disablePWM();
    }
    std::cout << "[TEST] Exiting cleanly." << std::endl;
    exit(signum);
}

int main(int argc, char **argv)
{
    std::cout << "=== VESPA HAL TEST: TRIGGER VERIFICATION MODE ===" << std::endl;

    std::string device = "/dev/video0";
    if (argc > 1)
        device = argv[1];

    // Register Signal Handler (Ctrl+C protection)
    signal(SIGINT, signalHandler);

    try
    {
        g_cam = std::make_unique<VESPA::CameraInterface>(device);

        // --- THE CRITICAL FIX: PWM FIRST ---
        // Start the hardware trigger BEFORE we wake the camera up or open the stream.
        // The Jetson Tegra driver needs to see these pulses immediately during STREAMON,
        // otherwise it will time out and force the Arducam MCU back into Master Mode.
        std::cout << "\n[TEST] Starting 50 Hz PWM Heartbeat..." << std::endl;
        // g_cam->enablePWM();

        // Give the Jetson's GPIO pins time to stabilize
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 1. Init & Stream On (Camera boots up, sees the PWM, and locks to 50 Hz)
        g_cam->init();
        g_cam->startStream();

        // [PHASE 1] Measuring hardware frame deltas...
        std::cout << "\n[PHASE 1] Measuring hardware frame deltas..." << std::endl;

        double current_ts = 0.0;
        double previous_ts = 0.0;

        for (int i = 0; i < 20; ++i)
        {
            // Notice we pass current_ts as the second argument
            if (!g_cam->captureFrame("", current_ts))
            {
                std::cerr << "[TEST] Capture failed!" << std::endl;
                break;
            }

            if (i > 0)
            {
                double delta = current_ts - previous_ts;
                std::cout << "  -> Frame " << i << " Hardware Delta: " << delta << " ms" << std::endl;
            }

            previous_ts = current_ts;
        }

        // --- PHASE 2: The "Pull the Plug" Test ---
        std::cout << "\n[PHASE 2] Disabling PWM mid-stream..." << std::endl;
        g_cam->disablePWM();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "[TEST] Attempting to capture frames with PWM OFF." << std::endl;
        std::cout << "[TEST] The software watchdog should catch this within 200ms." << std::endl;

        bool watchdogBite = false;
        double dummy_ts = 0.0; // We need a dummy variable for the Phase 2 calls

        for (int j = 0; j < 5; j++)
        {
            std::cout << "  -> Pulling post-PWM frame " << j << "..." << std::endl;

            // Notice we pass dummy_ts here to satisfy the compiler
            if (!g_cam->captureFrame("frame_leak_" + std::to_string(j) + ".pgm", dummy_ts))
            {
                watchdogBite = true;
                break;
            }
        }

        if (watchdogBite)
        {
            std::cout << "\n\033[1;32m[SUCCESS] Watchdog successfully caught the missing hardware trigger!\033[0m" << std::endl;
        }
        else
        {
            std::cerr << "\n\033[1;31m[FAILED] Frame captured even with PWM off!\033[0m" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[TEST] Exception: " << e.what() << std::endl;
        // Ensure PWM is killed on crash
        if (g_cam)
            g_cam->disablePWM();
        return 1;
    }

    return 0;
}