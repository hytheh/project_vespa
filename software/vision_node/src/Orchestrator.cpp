#include "Orchestrator.h"
#include <iostream>

namespace VESPA {
    Orchestrator::Orchestrator() {}
    Orchestrator::~Orchestrator() {}
    void Orchestrator::init() { std::cout << "[Orchestrator] Initialized." << std::endl; }
    void Orchestrator::tick() {}
}
