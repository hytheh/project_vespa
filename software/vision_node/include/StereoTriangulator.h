#pragma once

#include <vector>
#include "ArUcoTracker.h"

struct Target3D
{
    float X;       // Cartesian X in mm
    float Y;       // Cartesian Y in mm
    float Z;       // Depth in mm
    float left_cx; // 2D X-coordinate on the left image (for debug drawing)
    float left_cy; // 2D Y-coordinate on the left image (for debug drawing)
};

struct TriangulatorConfig
{
    float baseline_mm;
    float focal_length_px;
    float epipolar_tolerance_px;
    float min_disparity_px; // Prevents Z -> infinity
    float center_x_px;      // Principal point X (For upright: 800 / 2)
    float center_y_px;      // Principal point Y (For upright: 1280 / 2)
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