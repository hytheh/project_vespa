/**
 * @file main.cpp
 * @brief Project VESPA - Vision Node (Production Master)
 */

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <atomic>
#include <csignal>
#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

#include "HardwareSyncController.h"
#include "ArUcoTracker.h"
#include "StereoTriangulator.h"
#include "PredictiveTurretSight.h"

#include "vision_can.h"
#include "can_protocol.h"
#include "can_utils.h"

using json = nlohmann::json;
using namespace VESPA;

std::atomic<bool> keep_running(true);

void signalHandler(int)
{
    std::cout << "\n[VISION NODE] SIGINT received. Executing safe hardware teardown...\n";
    keep_running = false;
}

// --- HARDWARE ENDSTOPS (Radians) ---
const double MAX_PAN_RAD = 0.52;         // -30deg
const double MIN_PAN_RAD = -0.52;        // +30deg
const double MAX_TILT_RAD = 0.78;        // +45deg
const double MIN_TILT_RAD = -0.26;       // -15deg
const double FIRE_TOLERANCE_RAD = 0.035; // ~2.0 degrees

int main()
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "==========================================\n";
    std::cout << "      VESPA VISION NODE - PRODUCTION      \n";
    std::cout << "==========================================\n";

    // 1. BOOT NETWORK (Virtual Hardware Bus for Testing)
    VisionCan can_bus("can0");
    if (!can_bus.begin())
    {
        std::cerr << "[FATAL] SocketCAN interface failed to bind. Halting.\n";
        return -1;
    }
    std::cout << "[SYSTEM] CAN Bus [can0] Active.\n";

    // 2. BOOT OPTICS & MATH
    std::string config_path = "/home/hytheh/project_vespa/software/vision_node/config/";
    std::ifstream file(config_path + "stereovision_settings.json");
    if (!file.is_open())
    {
        std::cerr << "[FATAL] Could not open stereovision_settings.json\n";
        return -1;
    }
    json calib;
    file >> calib;

    float Fx = calib["camera_matrix_left"][0][0];
    float Fy = calib["camera_matrix_left"][1][1];
    float Cx = calib["camera_matrix_left"][0][2];
    float Cy = calib["camera_matrix_left"][1][2];
    float baseline_mm = std::abs((float)calib["T"][0][0]);

    cv::Mat K = (cv::Mat_<double>(3, 3) << Fx, 0, Cx, 0, Fy, Cy, 0, 0, 1);
    cv::Mat D = cv::Mat::zeros(1, 5, CV_64F);

    TriangulatorConfig t_config;
    t_config.baseline_mm = baseline_mm;
    t_config.focal_length_px = Fx;
    t_config.epipolar_tolerance_px = 150.0f;
    t_config.min_disparity_px = 1.0f;
    t_config.center_x_px = Cx;
    t_config.center_y_px = Cy;

    StereoTriangulator triangulator(t_config);
    ArUcoTracker tracker;
    PredictiveTurretSight sight(Fx, Fy, Cx, Cy, 0.15);

    HardwareSyncController hal("/dev/video0", "/dev/video1", config_path + "camera_settings.json");
    if (!hal.initializeRig())
        return -1;
    hal.armTrigger();

    std::cout << "[SYSTEM] Optical Frontend Locked. Node is LIVE.\n";

    double current_motor_pan = 0.0;
    double current_motor_tilt = 0.0;
    uint32_t frame_counter = 0;

    // --- PRODUCTION CORE LOOP ---
    while (keep_running)
    {
        long current_time_ms = cv::getTickCount() / (cv::getTickFrequency() / 1000);

        // A. DRAIN CAN QUEUE
        CAN_Frame_t rx_frame;
        while (can_bus.readFrame(&rx_frame))
        {
            if (rx_frame.id == CAN_ID_POS_MOTION)
            {
                MotionPosPayload_t motion_pos;
                if (CAN_Decode_MotionPos(&rx_frame, &motion_pos))
                {
                    current_motor_pan = (double)motion_pos.current_pan / ANGLE_SCALE_FACTOR;
                    current_motor_tilt = (double)motion_pos.current_tilt / ANGLE_SCALE_FACTOR;
                }
            }
        }

        // B. ACQUIRE HARDWARE SENSORS
        auto frame_pair = hal.captureSynchronizedPair();
        if (!frame_pair.valid)
            continue;

        auto raw_left = tracker.detect(frame_pair.left_data, 1280, 800);
        auto raw_right = tracker.detect(frame_pair.right_data, 1280, 800);

        bool target_locked = false;
        TargetPayload_t tx_target = {0};
        FirePayload_t tx_fire = {0};

        // C. COMPUTE FIRING SOLUTION
        if (!raw_left.empty() && !raw_right.empty())
        {
            auto undistort = [&](const std::vector<Target2D> &raw)
            {
                std::vector<Target2D> un = raw;
                std::vector<cv::Point2f> in, out;
                for (const auto &t : raw)
                    in.push_back(cv::Point2f(t.cx, t.cy));
                cv::undistortPoints(in, out, K, D, cv::noArray(), K);
                for (size_t i = 0; i < raw.size(); ++i)
                {
                    un[i].cx = out[i].x;
                    un[i].cy = out[i].y;
                }
                return un;
            };

            auto targets_3d = triangulator.triangulate(undistort(raw_left), undistort(raw_right));

            if (!targets_3d.empty())
            {
                auto closest = std::min_element(targets_3d.begin(), targets_3d.end(),
                                                [](const Target3D &a, const Target3D &b)
                                                { return a.Z < b.Z; });

                AimCommand cmd = sight.updateAndPredict(closest->X, closest->Y, closest->Z, current_time_ms);

                double safe_pan = std::clamp(cmd.pan_angle, MIN_PAN_RAD, MAX_PAN_RAD);
                double safe_tilt = std::clamp(cmd.tilt_angle, MIN_TILT_RAD, MAX_TILT_RAD);

                bool fire_auth = (std::abs(current_motor_pan - safe_pan) < FIRE_TOLERANCE_RAD) &&
                                 (std::abs(current_motor_tilt - safe_tilt) < FIRE_TOLERANCE_RAD);

                tx_target.pan_angle = (int16_t)(safe_pan * ANGLE_SCALE_FACTOR);
                tx_target.tilt_angle = (int16_t)(safe_tilt * ANGLE_SCALE_FACTOR);
                tx_target.depth_mm = (uint16_t)(std::clamp(closest->Z, 0.0f, 65535.0f));
                tx_target.is_locked = 1;
                tx_fire.fire_order = fire_auth ? 0x01 : 0x00;

                target_locked = true;
            }
        }

        if (!target_locked)
        {
            tx_target.pan_angle = (int16_t)(current_motor_pan * ANGLE_SCALE_FACTOR);
            tx_target.tilt_angle = (int16_t)(current_motor_tilt * ANGLE_SCALE_FACTOR);
            tx_target.depth_mm = 0;
            tx_target.is_locked = 0;
            tx_fire.fire_order = 0x00;
        }

        // D. BROADCAST TO NETWORK
        CAN_Frame_t tx_frame;
        if (CAN_Encode_TargetVec(&tx_frame, &tx_target))
            can_bus.sendFrame(&tx_frame);
        if (CAN_Encode_FireCmd(&tx_frame, &tx_fire))
            can_bus.sendFrame(&tx_frame);

        // E. PROTOCOL HEARTBEAT (1Hz at 60fps)
        if (frame_counter++ % 60 == 0)
        {
            HeartbeatPayload_t hb = {0x00};
            if (CAN_Encode_Heartbeat(&tx_frame, CAN_ID_HB_VISION, &hb))
                can_bus.sendFrame(&tx_frame);
        }

        hal.releaseSynchronizedPair(frame_pair);
    }

    // --- SAFE TEARDOWN ---
    hal.disarmTrigger();
    std::cout << "[SYSTEM] Hardware Locks Released. Node offline.\n";
    return 0;
}