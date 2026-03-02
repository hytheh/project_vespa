#include "StereoTriangulator.h"
#include <cmath>
#include <limits>
#include <algorithm>

StereoTriangulator::StereoTriangulator(const TriangulatorConfig &config)
    : config_(config) {}

std::vector<Target3D> StereoTriangulator::triangulate(
    const std::vector<Target2D> &left_targets,
    const std::vector<Target2D> &right_targets) const
{
    std::vector<Target3D> points_3d;
    points_3d.reserve(std::min(left_targets.size(), right_targets.size()));

    // Track which right targets have been assigned
    std::vector<bool> right_assigned(right_targets.size(), false);

    // Greedy Association
    for (size_t l = 0; l < left_targets.size(); ++l)
    {
        int best_r_idx = -1;
        float min_y_diff = std::numeric_limits<float>::max();

        for (size_t r = 0; r < right_targets.size(); ++r)
        {
            if (right_assigned[r])
                continue;

            float y_diff = std::abs(left_targets[l].cy - right_targets[r].cy);
            float disparity = left_targets[l].cx - right_targets[r].cx;

            // Enforce Epipolar constraint AND positive horizontal disparity
            if (y_diff < config_.epipolar_tolerance_px &&
                disparity > config_.min_disparity_px &&
                y_diff < min_y_diff)
            {

                min_y_diff = y_diff;
                best_r_idx = static_cast<int>(r);
            }
        }

        if (best_r_idx != -1)
        {
            // Match found, assign and compute 3D coordinate
            right_assigned[best_r_idx] = true;

            float disparity = left_targets[l].cx - right_targets[best_r_idx].cx;

            Target3D pt;
            pt.Z = (config_.focal_length_px * config_.baseline_mm) / disparity;
            pt.X = ((left_targets[l].cx - config_.center_x_px) * pt.Z) / config_.focal_length_px;
            pt.Y = ((left_targets[l].cy - config_.center_y_px) * pt.Z) / config_.focal_length_px;

            points_3d.push_back(pt);

#ifdef DEBUG_MODE
            std::cout << "[DEBUG] Matched L" << l << " to R" << best_r_idx
                      << " | dy: " << min_y_diff
                      << " | 3D(X,Y,Z): " << pt.X << ", " << pt.Y << ", " << pt.Z << " mm\n";
#endif
        }
    }

    return points_3d;
}