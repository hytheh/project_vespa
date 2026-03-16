/**
 * @file turret_tracker.cpp
 * @brief 2D Predictive Turret Sight using On-Gimbal Camera and CAN Bus
 * @details Single-target MVP tracking with Kalman filtering, latency compensation,
 * strict radian-based software endstops, and SocketCAN integration.
 */
#include "PredictiveTurretSight.h"
#include "vision_can.h"
#include "can_protocol.h"
#include "can_utils.h"

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <limits.h>
#include <algorithm>
#include <cmath>

using json = nlohmann::json;

// --- HARDWARE ENDSTOPS (Strictly in Radians) ---
const double MAX_PAN_RAD = M_PI / 2.0;              // +90 deg
const double MIN_PAN_RAD = -M_PI / 2.0;             // -90 deg
const double MAX_TILT_RAD = M_PI / 4.0;             // +45 deg
const double MIN_TILT_RAD = -20.0 * (M_PI / 180.0); // -20 deg

std::string resolveConfigPath()
{
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1)
    {
        std::string exePath(std::string(buffer, len));
        size_t build_pos = exePath.find("/build/");
        if (build_pos != std::string::npos)
        {
            return exePath.substr(0, build_pos) + "/config/stereovision_settings.json";
        }
    }
    return "../config/stereovision_settings.json";
}

int main()
{
    std::cout << "=== VESPA PREDICTIVE AIMING SYSTEM (MVP + CAN) ===" << std::endl;

    // 1. Initialize CAN Bus
    VisionCan can_bus("can0");
    if (!can_bus.begin())
    {
        std::cerr << "[FATAL] Failed to initialize SocketCAN on can0." << std::endl;
        return -1;
    }
    std::cout << "[TRACKER] CAN Bus Initialized on can0." << std::endl;

    // 2. Load Camera Intrinsics
    std::ifstream file(resolveConfigPath());
    if (!file.is_open())
    {
        std::cerr << "[ERROR] Missing calibration JSON!" << std::endl;
        return -1;
    }
    json calib;
    file >> calib;

    double Fx = calib["camera_matrix_left"][0][0];
    double Fy = calib["camera_matrix_left"][1][1];
    double Cx = calib["camera_matrix_left"][0][2];
    double Cy = calib["camera_matrix_left"][1][2];

    // 3. Initialize Predictive Sight (150ms guessed latency)
    PredictiveTurretSight sight(Fx, Fy, Cx, Cy, 0.15);

    // 4. Setup Video Capture
    cv::VideoCapture cap("/dev/video0", cv::CAP_V4L2);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 800);
    cap.set(cv::CAP_PROP_FPS, 60);

    if (!cap.isOpened())
    {
        std::cerr << "[FATAL] Could not open /dev/video0" << std::endl;
        return -1;
    }

    cv::Ptr<cv::aruco::Dictionary> dict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
    cv::Ptr<cv::aruco::DetectorParameters> params = cv::aruco::DetectorParameters::create();

    double current_turret_pan_rad = 0.0;
    double current_turret_tilt_rad = 0.0;

    std::cout << "[TRACKER] System armed. Scanning for targets..." << std::endl;

    cv::Mat frame;
    while (cap.read(frame))
    {
        long current_time_ms = cv::getTickCount() / (cv::getTickFrequency() / 1000);

        // --- 1. READ TURRET ANGLES FROM CAN BUS ---
        CAN_Frame_t rx_frame;
        // Drain the queue to ensure we have the absolute latest physical position
        while (can_bus.readFrame(&rx_frame))
        {
            MotionPosPayload_t motion_pos;
            if (CAN_Decode_MotionPos(&rx_frame, &motion_pos))
            {
                current_turret_pan_rad = (double)motion_pos.current_pan / ANGLE_SCALE_FACTOR;
                current_turret_tilt_rad = (double)motion_pos.current_tilt / ANGLE_SCALE_FACTOR;
            }
        }

        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;
        cv::aruco::detectMarkers(frame, dict, corners, ids, params);

        TargetPayload_t tx_payload;
        CAN_Frame_t tx_frame;

        if (!ids.empty())
        {
            cv::Point2f target_center(0, 0);
            for (int k = 0; k < 4; k++)
                target_center += corners[0][k];
            target_center *= 0.25f;

            AimCommand cmd = sight.updateAndPredict(
                target_center.x, target_center.y,
                current_turret_pan_rad, current_turret_tilt_rad,
                current_time_ms);

            // CLAMP to physical limits
            double safe_pan_rad = std::clamp(cmd.pan_angle, MIN_PAN_RAD, MAX_PAN_RAD);
            double safe_tilt_rad = std::clamp(cmd.tilt_angle, MIN_TILT_RAD, MAX_TILT_RAD);

            std::cout << "\r[LOCKED] Cmd Pan: " << std::fixed << std::setprecision(3) << safe_pan_rad
                      << " rad | Tilt: " << safe_tilt_rad << " rad      " << std::flush;

            // --- 2. SEND COMMANDS TO CAN BUS ---
            tx_payload.pan_angle = (int16_t)(safe_pan_rad * ANGLE_SCALE_FACTOR);
            tx_payload.tilt_angle = (int16_t)(safe_tilt_rad * ANGLE_SCALE_FACTOR);
            tx_payload.depth_mm = 0; // MVP ignores depth
            tx_payload.is_locked = 1;
        }
        else
        {
            std::cout << "\r[SCANNING...] No target visible.                                " << std::flush;

            // Transmit current known position with locked = 0 to tell motion node to hold
            tx_payload.pan_angle = (int16_t)(current_turret_pan_rad * ANGLE_SCALE_FACTOR);
            tx_payload.tilt_angle = (int16_t)(current_turret_tilt_rad * ANGLE_SCALE_FACTOR);
            tx_payload.depth_mm = 0;
            tx_payload.is_locked = 0;
        }

        // Encode and blast onto the network
        if (CAN_Encode_TargetVec(&tx_frame, &tx_payload))
        {
            can_bus.sendFrame(&tx_frame);
        }
    }

    cap.release();
    return 0;
}