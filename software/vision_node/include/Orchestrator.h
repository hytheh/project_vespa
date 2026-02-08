/** @file Orchestrator.h | @brief Main logic controller integrating Capture, Inference and Control. */
#ifndef VESPA_ORCHESTRATOR_H
#define VESPA_ORCHESTRATOR_H

namespace VESPA {
    class Orchestrator {
    public:
        Orchestrator();
        ~Orchestrator();
        void init();
        void tick(); // Main periodic execution
    };
}
#endif
