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
#include "vespa_worker.hpp"
#include "vespa_pipeline.hpp"

// --- Global Shutdown Signal ---
std::atomic<bool> g_running(true);
VespaPipeline* g_pipeline_ptr = nullptr;
VespaWorker* g_worker_ptr = nullptr;

// --- Signal Handler ---
void signal_handler(int signum) {
    std::cout << "\n[V.E.S.P.A] Interrupt signal (" << signum << ") received.\n";
    g_running = false;
    
    // Stop the GMainLoop if it's running
    // Note: In a real app, we might use a GUnixSignalWatch, 
    // but this is sufficient for the "Hello World"
    if (g_pipeline_ptr) {
        // We can't easily stop the GMainLoop from here without a reference to it
        // or using g_main_loop_quit() if we made the loop global.
        // For now, we rely on the application teardown sequence.
        exit(0); 
    }
}

int main(int argc, char** argv) {
    // 1. Register Signal Handlers
    std::signal(SIGINT, signal_handler); 
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== V.E.S.P.A. Vision Node Initializing ===" << std::endl;

    // 2. Initialize CAN Interface
    VespaCAN can_bus("can0"); 
    if (!can_bus.init()) {
        std::cerr << "[Warning] CAN Init failed. Running in offline mode." << std::endl;
    }

    // 3. Initialize Worker (The "Brain")
    VespaWorker worker;
    g_worker_ptr = &worker;
    worker.start();

    // 4. Initialize Pipeline (The "Eyes")
    VespaPipeline pipeline;
    g_pipeline_ptr = &pipeline;
    pipeline.init(argc, argv, &worker);

    // 5. Start The Hunt
    std::cout << "[V.E.S.P.A] System Ready. Starting Pipeline..." << std::endl; 
    pipeline.start();

    // 6. Enter Blocking Loop
    // This will block until an error or Ctrl+C
    pipeline.run();

    // 7. Cleanup
    std::cout << "[V.E.S.P.A] Shutting down..." << std::endl;
    worker.stop();
    
    return 0;
}