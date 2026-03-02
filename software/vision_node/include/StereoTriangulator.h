#pragma once

#include <vector>
#include "ArUcoTracker.h"

struct Target3D
{
    float X;
    float Y;
    float Z;
};

struct TriangulatorConfig
{
    float baseline_mm;
    float focal_length_px;
    float epipolar_tolerance_px;
    float min_disparity_px; // Prevents Z -> infinity
    float center_x_px;      // Principal point X (usually width / 2)
    float center_y_px;      // Principal point Y (usually height / 2)
};

class StereoTriangulator
{
public:
    explicit StereoTriangulator(const TriangulatorConfig &config);

    std::vector<Target3D> triangulate(
        const std::vector<Target2D> &left_targets,
        const std::vector<Target2D> &right_targets) const;

private:
    TriangulatorConfig config_;
};