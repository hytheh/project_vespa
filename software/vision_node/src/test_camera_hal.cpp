#include "CameraInterface.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <memory>

// Global pointer for signal handler
std::unique_ptr<VESPA::CameraInterface> g_cam;

void signalHandler(int signum)
{
    std::cout << "\n[TEST] Interrupt signal (" << signum << ") received." << std::endl;
    if (g_cam)
    {
        std::cout << "[TEST] Emergency PWM Shutdown..." << std::endl;
        g_cam->disablePWM();
    }
    exit(signum);
}

int main(int argc, char **argv)
{
    std::cout << "=== VESPA HAL TEST: ORCHESTRATOR MODE ===" << std::endl;

    std::string device = "/dev/video0";
    if (argc > 1)
        device = argv[1];

    // Register Signal Handler (Ctrl+C protection)
    signal(SIGINT, signalHandler);

    try
    {
        g_cam = std::make_unique<VESPA::CameraInterface>(device);

        // 1. Init & Stream On (PWM is OFF, Camera enters Slave Mode)
        g_cam->init();
        g_cam->startStream();

        // 2. Start Hardware Trigger
        std::cout << "[TEST] Enabling PWM Trigger..." << std::endl;
        g_cam->enablePWM();

        // Give it 100ms to spin up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 3. Capture Loop
        for (int i = 0; i < 10; ++i)
        {
            std::cout << "[TEST] Requesting frame " << i << "..." << std::endl;
            std::string fname = "frame_" + std::to_string(i) + ".pgm";

            if (!g_cam->captureFrame(fname))
            {
                std::cerr << "[TEST] Capture failed!" << std::endl;
                break;
            }
            std::cout << "  -> Saved: " << fname << std::endl;
        }

        std::cout << "[TEST] Complete. Shutting down..." << std::endl;

        // 4. Shutdown
        g_cam->disablePWM();
        g_cam->stopStream();
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