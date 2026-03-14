/**
 * @file stereo_tracker.cpp
 * @brief Live Production Pipeline: Hardware HAL -> ArUco Tracker -> Undistort -> Stereo Triangulator
 */

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>
#include <iomanip>
#include <opencv2/opencv.hpp>

#include "HardwareSyncController.h"
#include "ArUcoTracker.h"
#include "StereoTriangulator.h"

#ifdef DEBUG_MODE
#include <zmq.h>
#endif

using namespace VESPA;

std::atomic<bool> keep_running(true);

void signalHandler(int)
{
    std::cout << "\n[VESPA] Shutdown signal received. Halting pipeline...\n";
    keep_running = false;
}

// --- Quick, Zero-Dependency JSON Extractor ---
float extractJsonValue(const std::string &json_str, const std::string &key)
{
    size_t pos = json_str.find("\"" + key + "\"");
    if (pos == std::string::npos)
        return 0.0f;
    size_t colon_pos = json_str.find(":", pos);
    size_t comma_pos = json_str.find(",", colon_pos);
    size_t bracket_pos = json_str.find("]", colon_pos);

    size_t end_pos = (comma_pos < bracket_pos) ? comma_pos : bracket_pos;
    if (end_pos == std::string::npos)
        end_pos = json_str.find("\n", colon_pos);

    std::string val_str = json_str.substr(colon_pos + 1, end_pos - colon_pos - 1);
    val_str.erase(remove_if(val_str.begin(), val_str.end(), [](char c)
                            { return c == '[' || c == ']' || c == ' ' || c == '\n'; }),
                  val_str.end());
    try
    {
        return std::stof(val_str);
    }
    catch (...)
    {
        return 0.0f;
    }
}

int main()
{
    std::signal(SIGINT, signalHandler);
    std::cout << "=== VESPA Live Stereo Tracking Pipeline ===\n";

    // 1. Load Stereo Calibration Data
    std::string stereo_config_path = "/home/hytheh/project_vespa/software/vision_node/config/stereovision_settings.json";
    std::ifstream file(stereo_config_path);
    if (!file.is_open())
    {
        std::cerr << "[ERROR] Could not open stereovision_settings.json! Run calibration first.\n";
        return -1;
    }
    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    float baseline_mm = std::abs(extractJsonValue(json_str, "T"));
    float focal_length_px = extractJsonValue(json_str, "camera_matrix_left");

    std::cout << "[CONFIG] Loaded Baseline: " << baseline_mm << " mm\n";
    std::cout << "[CONFIG] Loaded Focal Length: " << focal_length_px << " px\n";

    // OpenCV matrices for Undistortion (Approximated for now)
    cv::Mat K = (cv::Mat_<double>(3, 3) << focal_length_px, 0, 400.0, 0, focal_length_px, 640.0, 0, 0, 1);
    cv::Mat D = cv::Mat::zeros(1, 5, CV_64F);

    // 2. Initialize Triangulator
    TriangulatorConfig config;
    config.baseline_mm = baseline_mm;
    config.focal_length_px = focal_length_px;
    config.epipolar_tolerance_px = 150.0f; // Loosened for raw uncalibrated hardware
    config.min_disparity_px = 1.0f;
    config.center_x_px = 400.0f;
    config.center_y_px = 640.0f;

    StereoTriangulator triangulator(config);
    ArUcoTracker tracker;

#ifdef DEBUG_MODE
    std::cout << "[DEBUG] Initializing ZeroMQ Live Visualization Stream on port 5556...\n";
    void *zmq_context = zmq_ctx_new();
    void *zmq_publisher = zmq_socket(zmq_context, ZMQ_PUB);
    zmq_bind(zmq_publisher, "tcp://*:5556");
#endif

    // 3. Boot the Hardware HAL
    std::cout << "[HAL] Booting Dual Arducam rig...\n";

    // Path to the Exposure/Gain optical profile
    std::string cam_settings_path = "/home/hytheh/project_vespa/software/vision_node/config/camera_settings.json";

    HardwareSyncController hal("/dev/video0", "/dev/video1", cam_settings_path);

    if (!hal.initializeRig())
    {
        std::cerr << "[FATAL] Failed to initialize hardware sync.\n";
        return -1;
    }

    hal.armTrigger(); // Start the 60Hz heartbeat

    std::cout << "\n[VESPA] Pipeline Live! Wave an ArUco marker. (Ctrl+C to quit)\n\n";

    // 4. Production Main Loop (60Hz)
    while (keep_running)
    {
        auto frame_pair = hal.captureSynchronizedPair();
        if (!frame_pair.valid)
            continue;

        auto raw_left_targets = tracker.detect(frame_pair.left_data, 1280, 800);
        auto raw_right_targets = tracker.detect(frame_pair.right_data, 1280, 800);

        // ADD THIS LINE TO DIAGNOSE WHAT THE 2D TRACKER SEES:
        std::cout << "Raw 2D Detections -> Left: " << raw_left_targets.size()
                  << " | Right: " << raw_right_targets.size() << "\r" << std::flush;

        std::vector<Target3D> targets_3d; // Empty by default

        // Only do the heavy triangulation math if we actually see markers in both eyes
        if (!raw_left_targets.empty() && !raw_right_targets.empty())
        {
            auto undistortTargets = [&](const std::vector<Target2D> &raw)
            {
                std::vector<Target2D> undistorted = raw;
                std::vector<cv::Point2f> pts_in, pts_out;
                for (const auto &t : raw)
                    pts_in.push_back(cv::Point2f(t.cx, t.cy));
                cv::undistortPoints(pts_in, pts_out, K, D, cv::noArray(), K);
                for (size_t i = 0; i < raw.size(); ++i)
                {
                    undistorted[i].cx = pts_out[i].x;
                    undistorted[i].cy = pts_out[i].y;
                }
                return undistorted;
            };

            auto clean_left = undistortTargets(raw_left_targets);
            auto clean_right = undistortTargets(raw_right_targets);
            targets_3d = triangulator.triangulate(clean_left, clean_right);
        }

#ifdef DEBUG_MODE
        // ALWAYS broadcast the frame to the UI so we have a live viewfinder!
        // Part 1: Send BOTH Raw Buffers (Left then Right)
        zmq_msg_t msg_frame;
        zmq_msg_init_size(&msg_frame, 2560 * 800); // 2MB combined payload
        uint8_t *combined_buf = (uint8_t *)zmq_msg_data(&msg_frame);

        // Copy Left Frame to the first half
        memcpy(combined_buf, frame_pair.left_data, 1280 * 800);
        // Copy Right Frame to the second half
        memcpy(combined_buf + (1280 * 800), frame_pair.right_data, 1280 * 800);

        zmq_msg_send(&msg_frame, zmq_publisher, ZMQ_SNDMORE);
        zmq_msg_close(&msg_frame);

        // Part 2: Send JSON Targets (Will safely send "[]" if none are found)
        std::string json_payload = "[";
        for (size_t i = 0; i < targets_3d.size(); ++i)
        {
            json_payload += "{\"x\":" + std::to_string(targets_3d[i].X) +
                            ",\"y\":" + std::to_string(targets_3d[i].Y) +
                            ",\"z\":" + std::to_string(targets_3d[i].Z) +
                            ",\"cx\":" + std::to_string(targets_3d[i].left_cx) +
                            ",\"cy\":" + std::to_string(targets_3d[i].left_cy) + "}";
            if (i < targets_3d.size() - 1)
                json_payload += ",";
        }
        json_payload += "]";

        zmq_msg_t msg_json;
        zmq_msg_init_size(&msg_json, json_payload.size());
        memcpy(zmq_msg_data(&msg_json), json_payload.c_str(), json_payload.size());
        zmq_msg_send(&msg_json, zmq_publisher, 0);
        zmq_msg_close(&msg_json);
#else
        // Headless production output
        for (const auto &t : targets_3d)
        {
            std::cout << "[TARGET] Lock -> X: " << (int)t.X
                      << "mm | Y: " << (int)t.Y
                      << "mm | Z: " << (int)t.Z << "mm\n";
        }
#endif

        // --- CRITICAL MEMORY MANAGEMENT ---
        hal.releaseSynchronizedPair(frame_pair);
    }

    hal.disarmTrigger(); // Shut off the PWM hardware trigger

#ifdef DEBUG_MODE
    zmq_close(zmq_publisher);
    zmq_ctx_destroy(zmq_context);
#endif

    return 0;
}