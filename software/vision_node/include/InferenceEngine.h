/** @file InferenceEngine.h | @brief Wraps TensorRT engine execution. */
#ifndef VESPA_INFERENCEENGINE_H
#define VESPA_INFERENCEENGINE_H

namespace VESPA {
    class InferenceEngine {
    public:
        InferenceEngine();
        ~InferenceEngine();
        void init();
        void tick(); // Main periodic execution
    };
}
#endif
