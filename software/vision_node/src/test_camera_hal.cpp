#include "CameraInterface.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <memory>
#include <iomanip>
#include <cstdlib> // Required for std::system

// Global pointer for signal handler
std::unique_ptr<VESPA::CameraInterface> g_cam;

// --- HARDWARE SAFETY GATE ---
void enforceHardwareSafety(const std::string &deviceNode)
{
    std::cout << "\n\033[1;31m============================================================\033[0m\n";
    std::cout << "\033[1;31m                  CRITICAL HARDWARE WARNING                 \033[0m\n";
    std::cout << "\033[1;31m============================================================\033[0m\n";
    std::cout << "You are launching the SINGLE CAMERA diagnostic on " << deviceNode << ".\n";
    std::cout << "Because the cameras share a Y-harness, testing one camera while\n";
    std::cout << "the other is plugged in will cause a dead short and BURN OUT THE SENSOR.\n\n";
    std::cout << "Please physically unplug the MIPI ribbon for the OTHER camera now.\n";
    std::cout << "To confirm it is safe to proceed, type the word 'UNPLUGGED': ";

    std::string input;
    std::cin >> input;

    if (input != "UNPLUGGED")
    {
        std::cerr << "\n\033[1;31m[ABORT]\033[0m Safety check failed. Exiting to protect hardware.\n\n";
        exit(EXIT_FAILURE);
    }
    std::cout << "\n\033[1;32m[SAFE]\033[0m Proceeding with single-camera test...\n\n";
}

// --- RAW PWM HELPER ---
void disableRawPWM()
{
    std::system("echo 0 > /sys/class/pwm/pwmchip3/pwm0/duty_cycle 2>/dev/null");
    std::system("echo 0 > /sys/class/pwm/pwmchip3/pwm0/enable 2>/dev/null");
}

void signalHandler(int signum)
{
    std::cout << "\n\n[TEST] Interrupt signal (" << signum << ") received." << std::endl;
    std::cout << "[TEST] Emergency PWM Shutdown..." << std::endl;
    disableRawPWM();
    std::cout << "[TEST] Exiting cleanly." << std::endl;
    exit(signum);
}

int main(int argc, char **argv)
{
    std::cout << "=== VESPA HAL TEST: TRIGGER VERIFICATION MODE ===" << std::endl;

    std::string device = "/dev/video0";
    if (argc > 1)
        device = argv[1];

    // 1. Force the user to confirm the hardware is safe
    enforceHardwareSafety(device);

    // Register Signal Handler (Ctrl+C protection)
    signal(SIGINT, signalHandler);

    try
    {
        g_cam = std::make_unique<VESPA::CameraInterface>(device);

        // 2. Export PWM and clamp to 0V instantly
        std::system("echo 0 > /sys/class/pwm/pwmchip3/export 2>/dev/null");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::system("echo 16666666 > /sys/class/pwm/pwmchip3/pwm0/period 2>/dev/null");
        disableRawPWM();

        // 3. Init & Stream On (Camera boots up in Master Mode)
        g_cam->init();
        g_cam->startStream();

        // 4. Force Slave Mode explicitly for this test
        std::cout << "[TEST] Forcing Slave Mode via v4l2-ctl..." << std::endl;
        std::string cmd = "v4l2-ctl -d " + device + " -c trigger_mode=1";
        std::system(cmd.c_str());

        // 5. Flush the garbage frames from Master Mode
        g_cam->flushQueue();

        // 6. Arm the Hardware Trigger (60 Hz)
        std::cout << "\n[TEST] Starting 60 Hz PWM Heartbeat..." << std::endl;
        std::system("echo 1666666 > /sys/class/pwm/pwmchip3/pwm0/duty_cycle");
        std::system("echo 1 > /sys/class/pwm/pwmchip3/pwm0/enable");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // [PHASE 1] Measuring hardware frame deltas...
        std::cout << "\n[PHASE 1] Measuring hardware frame deltas..." << std::endl;

        double current_ts = 0.0;
        double previous_ts = 0.0;

        for (int i = 0; i < 20; ++i)
        {
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
        disableRawPWM();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "[TEST] Attempting to capture frames with PWM OFF." << std::endl;
        std::cout << "[TEST] The software watchdog should catch this within 200ms." << std::endl;

        bool watchdogBite = false;
        double dummy_ts = 0.0;

        for (int j = 0; j < 5; j++)
        {
            std::cout << "  -> Pulling post-PWM frame " << j << "..." << std::endl;

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
        disableRawPWM();
        return 1;
    }

    // Clean shutdown
    disableRawPWM();
    return 0;
}