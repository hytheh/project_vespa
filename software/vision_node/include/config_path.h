/**
 * @file config_path.h
 * @brief Resolve config-file paths relative to the running executable, so no
 *        hardcoded usernames leak into the binaries. Mirrors the logic already
 *        used in calibration_bridge.cpp / sparse_tracker.cpp.
 */
#pragma once

#include <string>
#include <unistd.h>
#include <limits.h>

/**
 * @brief Absolute path to a file in vision_node/config/, derived from the
 *        executable location. Assumes the binary runs from a "build/" directory
 *        (see CMakeLists.txt layout). Falls back to "../config/<filename>".
 */
inline std::string resolveConfigPath(const std::string &filename)
{
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1)
    {
        buffer[len] = '\0';
        std::string exePath(buffer);
        size_t build_pos = exePath.find("/build/");
        if (build_pos != std::string::npos)
        {
            return exePath.substr(0, build_pos) + "/config/" + filename;
        }
    }
    return "../config/" + filename; // Fallback
}
