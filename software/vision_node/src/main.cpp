/**
 * @file main.cpp
 * @brief Minimal Bootstrapper for Vision Node
 */
#include "Orchestrator.h"
#include <iostream>
#include <signal.h>

bool volatile g_running = true;

void signal_handler(int signum) {
    g_running = false;
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);
    
    VESPA::Orchestrator app;
    
    try {
        app.init();
        
        while(g_running) {
            app.tick();
            // TODO: Implement proper sleep/wait for interrupt here
        }
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL FAILURE: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
