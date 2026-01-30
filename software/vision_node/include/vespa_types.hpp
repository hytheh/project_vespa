/**
 * @file vespa_types.hpp
 * @brief Common data structures for the Vision Node internal logic.
 */

#ifndef VESPA_TYPES_HPP
#define VESPA_TYPES_HPP

#include <cstdint>

/**
 * @struct VespaWorkItem
 * @brief Represents a detection to be processed by the worker thread.
 *        Contains bounding box information and metadata.
 */
struct VespaWorkItem {
    float x;            ///< Bounding box top-left X coordinate
    float y;            ///< Bounding box top-left Y coordinate
    float w;        ///< Bounding box width
    float h;       ///< Bounding box height
    int class_id;       ///< Detected object class ID
    float confidence;   ///< Detection confidence score
    uint64_t timestamp; ///< Timestamp of the frame (e.g., nanoseconds)
    uint64_t object_id; ///< Unique tracking ID for the object
};

#endif // VESPA_TYPES_HPP