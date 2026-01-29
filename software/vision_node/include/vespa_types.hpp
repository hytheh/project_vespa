/**
 * @file vespa_types.hpp
 * @brief Common data structures for the Vision Node internal logic.
 */

#ifndef VESPA_TYPES_HPP
#define VESPA_TYPES_HPP

#include <cstdint>

/**
 * @struct VespaWorkItem
 * @brief Represents a single detection target to be processed.
 */
struct VespaWorkItem {
    uint64_t timestamp_ns; ///< Capture timestamp in nanoseconds
    uint64_t object_id;    ///< Unique ID from the Tracker
    
    // 2D Bounding Box (Camera Left)
    float bbox_x;
    float bbox_y;
    float bbox_w;
    float bbox_h;

    float confidence;      ///< Detection confidence (0.0 - 1.0)
};

#endif // VESPA_TYPES_HPP