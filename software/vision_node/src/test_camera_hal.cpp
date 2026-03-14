/**
 * @file test_camera_hal.cpp
 * @brief Standalone test to validate the V4L2 watchdog and PWM lifecycles.
 */

#include "CameraInterface.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <memory>
#include <iomanip>
#include <fstream>
#include <cstdlib>

std::unique_ptr<VESPA::CameraInterface> g_cam;

void enforceHardwareSafety(const std::string &deviceNode)
{
    std::cout << "\n\033[1;31m============================================================\033[0m\n";
    std::cout << "\033[1;31m                  CRITICAL HARDWARE WARNING                 \033[0m\n";
    std::cout << "\033[1;31m============================================================\033[0m\n";
    std::cout << "You are launching the SINGLE CAMERA diagnostic on " << deviceNode << ".\n";
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

void disableRawPWM()
{
    std::system("echo 0 > /sys/class/pwm/pwmchip3/pwm0/duty_cycle 2>/dev/null");
    std::system("echo 0 > /sys/class/pwm/pwmchip3/pwm0/enable 2>/dev/null");
}

void signalHandler(int signum)
{
    std::cout << "\n\n[TEST] Interrupt signal (" << signum << ") received." << std::endl;
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

    enforceHardwareSafety(device);
    signal(SIGINT, signalHandler);

    try
    {
        g_cam = std::make_unique<VESPA::CameraInterface>(device);

        std::system("echo 0 > /sys/class/pwm/pwmchip3/export 2>/dev/null");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::system("echo 16666666 > /sys/class/pwm/pwmchip3/pwm0/period 2>/dev/null");
        disableRawPWM();

        g_cam->init();
        g_cam->startStream();

        std::cout << "[TEST] Forcing Slave Mode via v4l2-ctl..." << std::endl;
        std::string cmd = "v4l2-ctl -d " + device + " -c trigger_mode=1";
        std::system(cmd.c_str());

        g_cam->flushQueue();

        std::cout << "\n[TEST] Starting 60 Hz PWM Heartbeat..." << std::endl;
        std::system("echo 1666666 > /sys/class/pwm/pwmchip3/pwm0/duty_cycle");
        std::system("echo 1 > /sys/class/pwm/pwmchip3/pwm0/enable");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::cout << "\n[PHASE 1] Measuring hardware frame deltas..." << std::endl;
        double current_ts = 0.0, previous_ts = 0.0;
        uint8_t *frame_ptr = nullptr;
        int frame_idx = -1;

        for (int i = 0; i < 20; ++i)
        {
            // New API
            if (!g_cam->captureFrame(current_ts, frame_ptr, frame_idx))
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

            // NEW: Release memory
            g_cam->releaseFrame(frame_idx);
        }

        std::cout << "\n[PHASE 2] Disabling PWM mid-stream..." << std::endl;
        disableRawPWM();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "[TEST] Attempting to capture frames with PWM OFF." << std::endl;
        bool watchdogBite = false;

        for (int j = 0; j < 5; j++)
        {
            std::cout << "  -> Pulling post-PWM frame " << j << "..." << std::endl;

            // New API
            if (!g_cam->captureFrame(current_ts, frame_ptr, frame_idx))
            {
                watchdogBite = true;
                break;
            }

            // We caught a frame while PWM was off (bad!). Dump it to disk to debug.
            std::string fname = "frame_leak_" + std::to_string(j) + ".pgm";
            std::ofstream file(fname, std::ios::binary);
            file << "P5\n"
                 << VESPA::SENSOR_WIDTH << " " << VESPA::SENSOR_HEIGHT << "\n255\n";
            file.write(reinterpret_cast<char *>(frame_ptr), VESPA::SENSOR_WIDTH * VESPA::SENSOR_HEIGHT);
            file.close();

            g_cam->releaseFrame(frame_idx);
        }

        if (watchdogBite)
            std::cout << "\n\033[1;32m[SUCCESS] Watchdog successfully caught the missing hardware trigger!\033[0m" << std::endl;
        else
            std::cerr << "\n\033[1;31m[FAILED] Frame captured even with PWM off!\033[0m" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[TEST] Exception: " << e.what() << std::endl;
        disableRawPWM();
        return 1;
    }

    disableRawPWM();
    return 0;
}