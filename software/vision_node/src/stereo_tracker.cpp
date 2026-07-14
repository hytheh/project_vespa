/**
 * @file stereo_tracker.cpp
 * @brief Live Production Pipeline with 3D Kalman Predictive Aiming
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
#include "PredictiveTurretSight.h" // INJECTED KALMAN FILTER
#include "config_path.h"

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

// Quick JSON Extractor
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

    std::string stereo_config_path = resolveConfigPath("stereovision_settings.json");
    std::ifstream file(stereo_config_path);
    if (!file.is_open())
        return -1;
    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    float baseline_mm = std::abs(extractJsonValue(json_str, "T"));
    float focal_length_px = extractJsonValue(json_str, "camera_matrix_left");

    cv::Mat K = (cv::Mat_<double>(3, 3) << focal_length_px, 0, 400.0, 0, focal_length_px, 640.0, 0, 0, 1);
    cv::Mat D = cv::Mat::zeros(1, 5, CV_64F);

    TriangulatorConfig config;
    config.baseline_mm = baseline_mm;
    config.focal_length_px = focal_length_px;
    config.epipolar_tolerance_px = 150.0f;
    config.min_disparity_px = 1.0f;
    config.center_x_px = 400.0;
    config.center_y_px = 640.0;

    StereoTriangulator triangulator(config);
    ArUcoTracker tracker;

    // INITIALIZE KALMAN SIGHT (150ms guessed latency)
    PredictiveTurretSight sight(focal_length_px, focal_length_px, config.center_x_px, config.center_y_px, 0.15);

#ifdef DEBUG_MODE
    void *zmq_context = zmq_ctx_new();
    void *zmq_publisher = zmq_socket(zmq_context, ZMQ_PUB);

    // PREVENT HANG: Tell ZMQ to instantly drop unsent frames on shutdown
    int linger = 0;
    zmq_setsockopt(zmq_publisher, ZMQ_LINGER, &linger, sizeof(linger));

    zmq_bind(zmq_publisher, "tcp://*:5556");
#endif

    std::string cam_settings_path = resolveConfigPath("camera_settings.json");
    HardwareSyncController hal("/dev/video0", "/dev/video1", cam_settings_path);
    if (!hal.initializeRig())
        return -1;

    hal.armTrigger();
    std::cout << "\n[VESPA] Pipeline Live! Wave an ArUco marker. (Ctrl+C to quit)\n\n";

    while (keep_running)
    {
        long current_time_ms = cv::getTickCount() / (cv::getTickFrequency() / 1000);
        auto frame_pair = hal.captureSynchronizedPair();
        if (!frame_pair.valid)
            continue;

        auto raw_left_targets = tracker.detect(frame_pair.left_data, 1280, 800);
        auto raw_right_targets = tracker.detect(frame_pair.right_data, 1280, 800);

        std::vector<Target3D> targets_3d;
        AimCommand current_cmd; // Hold the prediction
        bool target_locked = false;

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

            targets_3d = triangulator.triangulate(undistortTargets(raw_left_targets), undistortTargets(raw_right_targets));

            if (!targets_3d.empty())
            {
                // Prioritize closest target
                auto closest = std::min_element(targets_3d.begin(), targets_3d.end(),
                                                [](const Target3D &a, const Target3D &b)
                                                { return a.Z < b.Z; });

                // Feed 3D coordinates into Kalman filter
                current_cmd = sight.updateAndPredict(closest->X, closest->Y, closest->Z, current_time_ms);
                target_locked = true;
            }
        }

#ifdef DEBUG_MODE
        // Part 1: Send Raw Video
        zmq_msg_t msg_frame;
        zmq_msg_init_size(&msg_frame, 2560 * 800);
        uint8_t *combined_buf = (uint8_t *)zmq_msg_data(&msg_frame);
        memcpy(combined_buf, frame_pair.left_data, 1280 * 800);
        memcpy(combined_buf + (1280 * 800), frame_pair.right_data, 1280 * 800);
        zmq_msg_send(&msg_frame, zmq_publisher, ZMQ_SNDMORE);
        zmq_msg_close(&msg_frame);

        // Part 2: Formulate JSON with New Predictive Overlays
        std::string json_payload = "[";
        if (target_locked)
        {
            auto &t = targets_3d[0]; // For MVP UI, just send the tracked one
            json_payload += "{\"x\":" + std::to_string(t.X) +
                            ",\"y\":" + std::to_string(t.Y) +
                            ",\"z\":" + std::to_string(t.Z) +
                            ",\"cx\":" + std::to_string(t.left_cx) +
                            ",\"cy\":" + std::to_string(t.left_cy) +
                            ",\"pred_x\":" + std::to_string(current_cmd.pred_X) +
                            ",\"pred_y\":" + std::to_string(current_cmd.pred_Y) +
                            ",\"pred_z\":" + std::to_string(current_cmd.pred_Z) +
                            ",\"pred_cx\":" + std::to_string(current_cmd.pred_cx) +
                            ",\"pred_cy\":" + std::to_string(current_cmd.pred_cy) + "}";
        }
        json_payload += "]";

        zmq_msg_t msg_json;
        zmq_msg_init_size(&msg_json, json_payload.size());
        memcpy(zmq_msg_data(&msg_json), json_payload.c_str(), json_payload.size());
        zmq_msg_send(&msg_json, zmq_publisher, 0);
        zmq_msg_close(&msg_json);
#else
        if (target_locked)
            std::cout << "\r[LOCKED] Z: " << (int)targets_3d[0].Z << "mm | Lead Pan: " << current_cmd.pan_angle << " rad    " << std::flush;
        else
            std::cout << "\r[SCANNING...]                                              " << std::flush;
#endif
        hal.releaseSynchronizedPair(frame_pair);
    }

    hal.disarmTrigger();
#ifdef DEBUG_MODE
    zmq_close(zmq_publisher);
    zmq_ctx_destroy(zmq_context);
#endif
    return 0;
}