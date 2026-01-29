/**
 * @file main.cpp
 * @brief Entry Point for the V.E.S.P.A. Vision System.
 *
 * @section DESCRIPTION
 * Initializes the "Hunter" application. It orchestrates:
 * 1. The GStreamer/DeepStream Main Loop.
 * 2. The SocketCAN Communication Thread.
 * 3. The Unix Signal Handler (SIGINT/SIGTERM) for safe shutdown.
 *
 * @section FLOW
 * - Init CAN Interface -> Init Pipeline -> Start Main Loop -> Wait for EOS/Error.
 *
 * @note This file is kept minimal. Logic is delegated to `vespa_pipeline` class.
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include "vespa_can.hpp"

// --- Global Shutdown Signal ---
std::atomic<bool> g_running(true);

// --- Signal Handler ---
void signal_handler(int signum) {
    std::cout << "\n[V.E.S.P.A] Interrupt signal (" << signum << ") received.\n";
    g_running = false;
}

int main(int argc, char** argv) {
    // 1. Register Signal Handlers
    std::signal(SIGINT, signal_handler); 
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== V.E.S.P.A. Vision Node Initializing ===" << std::endl;

    // 2. Initialize CAN Interface
    VespaCAN can_bus("can0"); 
    if (!can_bus.init()) {
        std::cerr << "[Error] CAN Init failed. Continuing in offline mode..." << std::endl;
    }

    // 3. TODO: Initialize DeepStream Pipeline
    // VespaPipeline pipeline;
    // pipeline.init(argc, argv);

    // 4. Main Application Loop
    std::cout << "[V.E.S.P.A] System Ready. Entering Main Loop..." << std::endl; 
    
    while (g_running) {
        // Placeholder for main loop logic
        // If using GMainLoop, this while loop might be replaced by loop.run()
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[V.E.S.P.A] Shutdown Complete." << std::endl;

    return 0;
}