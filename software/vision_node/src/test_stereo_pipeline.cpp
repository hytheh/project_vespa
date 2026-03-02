/**
 * @file test_stereo_pipeline.cpp
 * @brief Synthetic test harness for validating the ArUcoTracker and StereoTriangulator modules.
 * * This executable generates artificial 8-bit grayscale buffers with simulated ArUco markers,
 * bypassing the physical CameraInterface to test the epipolar data association logic
 * and 3D triangulation math. It includes a test case for target occlusion.
 */

#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

#include "ArUcoTracker.h"
#include "StereoTriangulator.h"

/**
 * @brief Draws a synthetic ArUco marker onto a background image buffer.
 * * @param bg Reference to the background cv::Mat (must be CV_8UC1).
 * @param id The dictionary ID of the marker to generate.
 * @param top_left_x The X coordinate of the top-left corner of the marker.
 * @param top_left_y The Y coordinate of the top-left corner of the marker.
 * @param size The size (width and height) of the marker in pixels.
 */
void drawSyntheticMarker(cv::Mat& bg, int id, int top_left_x, int top_left_y, int size) {
    cv::Ptr<cv::aruco::Dictionary> dict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
    cv::Mat marker;
    // Downgraded for OpenCV 4.5.4 compatibility
    cv::aruco::drawMarker(dict, id, size, marker, 1); 
    
    cv::Rect roi(top_left_x, top_left_y, size, size);
    marker.copyTo(bg(roi));
}

/**
 * @brief Main execution function for the stereo pipeline test.
 * @return int Exit status.
 */
int main() {
    std::cout << "--- Project VESPA: Phase 3 Stereo Pipeline Test ---\n\n";

    const int WIDTH = 1280;
    const int HEIGHT = 800;

    // 1. Create synthetic "raw" 8-bit grayscale buffers initialized to white (255)
    cv::Mat left_frame(HEIGHT, WIDTH, CV_8UC1, cv::Scalar(255));
    cv::Mat right_frame(HEIGHT, WIDTH, CV_8UC1, cv::Scalar(255));

    // Target A (Simulated Hornet 1) - Perfect alignment
    drawSyntheticMarker(left_frame, 0, 350, 350, 100); //ID0 marker
    drawSyntheticMarker(right_frame, 0, 300, 350, 100); // Disparity: 50px

    // Target B (Simulated Hornet 2) - Y-axis jitter to test Epipolar Tolerance
    drawSyntheticMarker(left_frame, 0, 750, 250, 100); //ID0 marker
    drawSyntheticMarker(right_frame, 0, 670, 252, 100); // Disparity: 80px, Y-offset: 2px

    // Target C (Simulated Hornet 3) - Occlusion Test (Visible left, blocked right)
    drawSyntheticMarker(left_frame, 2, 100, 600, 100); //ID2 marker

    cv::imwrite("synthetic_left_eye.png", left_frame);
    cv::imwrite("synthetic_right_eye.png", right_frame);
    std::cout << "Saved synthetic frames to disk.\n\n";

    // 2. Initialize the Tracker
    ArUcoTracker tracker;

    std::cout << "[1/3] Extracting Left Targets...\n";
    auto left_targets = tracker.detect(left_frame.data, WIDTH, HEIGHT);
    
    std::cout << "[2/3] Extracting Right Targets...\n";
    auto right_targets = tracker.detect(right_frame.data, WIDTH, HEIGHT);

    std::cout << "Detected " << left_targets.size() << " left targets and " 
              << right_targets.size() << " right targets.\n\n";

    // 3. Configure and Run the Triangulator
    TriangulatorConfig config;
    config.baseline_mm = 120.0f;           
    config.focal_length_px = 800.0f;       
    config.epipolar_tolerance_px = 5.0f;   
    config.min_disparity_px = 1.0f;
    config.center_x_px = WIDTH / 2.0f;
    config.center_y_px = HEIGHT / 2.0f;

    StereoTriangulator triangulator(config);
    
    std::cout << "[3/3] Triangulating 3D Coordinates...\n";
    auto points_3d = triangulator.triangulate(left_targets, right_targets);

    // 4. Output Results
    std::cout << "\n--- Final 3D Coordinates (Relative to Left Camera) ---\n";
    for (size_t i = 0; i < points_3d.size(); ++i) {
        std::cout << "Valid 3D Target " << i + 1 << ": "
                  << "X=" << points_3d[i].X << "mm, "
                  << "Y=" << points_3d[i].Y << "mm, "
                  << "Z=" << points_3d[i].Z << "mm\n";
    }
    
    std::cout << "\nExpected output: 2 valid 3D targets (Target C should be correctly discarded due to right-eye occlusion).\n";

    return 0;
}