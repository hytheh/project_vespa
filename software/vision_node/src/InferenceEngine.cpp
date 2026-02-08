#include "InferenceEngine.h"
#include <iostream>

namespace VESPA {
    InferenceEngine::InferenceEngine() {}
    InferenceEngine::~InferenceEngine() {}
    void InferenceEngine::init() { std::cout << "[InferenceEngine] Initialized." << std::endl; }
    void InferenceEngine::tick() {}
}
