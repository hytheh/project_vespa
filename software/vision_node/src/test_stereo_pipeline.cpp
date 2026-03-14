/**
 * @file test_stereo_pipeline.cpp
 * @brief Synthetic test harness for validating the ArUcoTracker and StereoTriangulator modules.
 */

#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

#include "ArUcoTracker.h"
#include "StereoTriangulator.h"

void drawSyntheticMarker(cv::Mat &bg, int id, int top_left_x, int top_left_y, int size)
{
    cv::Ptr<cv::aruco::Dictionary> dict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
    cv::Mat marker;
    // Downgraded for OpenCV 4.5.4 compatibility
    cv::aruco::drawMarker(dict, id, size, marker, 1);

    cv::Rect roi(top_left_x, top_left_y, size, size);
    marker.copyTo(bg(roi));
}

int main()
{
    std::cout << "--- Project VESPA: Phase 3 Stereo Pipeline Test ---\n\n";

    // PHYSICAL sideways buffer sizes
    const int RAW_WIDTH = 1280;
    const int RAW_HEIGHT = 800;

    // 1. Create synthetic "raw" 8-bit grayscale buffers initialized to white (255)
    cv::Mat left_frame(RAW_HEIGHT, RAW_WIDTH, CV_8UC1, cv::Scalar(255));
    cv::Mat right_frame(RAW_HEIGHT, RAW_WIDTH, CV_8UC1, cv::Scalar(255));

    // Draw markers in the raw SIDEWAYS coordinate space.
    // Tracker will mathematically rotate them to upright.
    drawSyntheticMarker(left_frame, 0, 350, 350, 100);
    drawSyntheticMarker(right_frame, 0, 350, 300, 100);

    drawSyntheticMarker(left_frame, 0, 750, 250, 100);
    drawSyntheticMarker(right_frame, 0, 752, 170, 100);

    drawSyntheticMarker(left_frame, 2, 100, 600, 100);

    cv::imwrite("synthetic_left_eye.png", left_frame);
    cv::imwrite("synthetic_right_eye.png", right_frame);
    std::cout << "Saved synthetic frames to disk.\n\n";

    // 2. Initialize the Tracker
    ArUcoTracker tracker;

    std::cout << "[1/3] Extracting Left Targets...\n";
    auto left_targets = tracker.detect(left_frame.data, RAW_WIDTH, RAW_HEIGHT);

    std::cout << "[2/3] Extracting Right Targets...\n";
    auto right_targets = tracker.detect(right_frame.data, RAW_WIDTH, RAW_HEIGHT);

    std::cout << "Detected " << left_targets.size() << " left targets and "
              << right_targets.size() << " right targets.\n\n";

    // 3. Configure and Run the Triangulator
    TriangulatorConfig config;
    config.baseline_mm = 120.0f;
    config.focal_length_px = 800.0f;
    config.epipolar_tolerance_px = 5.0f;
    config.min_disparity_px = 1.0f;

    // Note: Principal centers reflect the NEW rotated upright resolution (800x1280)
    config.center_x_px = 800.0f / 2.0f;  // 400
    config.center_y_px = 1280.0f / 2.0f; // 640

    StereoTriangulator triangulator(config);

    std::cout << "[3/3] Triangulating 3D Coordinates...\n";
    auto points_3d = triangulator.triangulate(left_targets, right_targets);

    // 4. Output Results
    std::cout << "\n--- Final 3D Coordinates (Relative to Left Camera) ---\n";
    for (size_t i = 0; i < points_3d.size(); ++i)
    {
        std::cout << "Valid 3D Target " << i + 1 << ": "
                  << "X=" << points_3d[i].X << "mm, "
                  << "Y=" << points_3d[i].Y << "mm, "
                  << "Z=" << points_3d[i].Z << "mm\n";
    }

    std::cout << "\nExpected output: 2 valid 3D targets (Target C should be correctly discarded due to right-eye occlusion).\n";

    return 0;
}