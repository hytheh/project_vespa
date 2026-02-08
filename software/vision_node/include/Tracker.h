/** @file Tracker.h | @brief Kalman Filter implementation. */
#ifndef VESPA_TRACKER_H
#define VESPA_TRACKER_H

namespace VESPA {
    class Tracker {
    public:
        Tracker();
        ~Tracker();
        void init();
        void tick(); // Main periodic execution
    };
}
#endif
