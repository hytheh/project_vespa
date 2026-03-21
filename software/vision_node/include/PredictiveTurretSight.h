/**
 * @file PredictiveTurretSight.h
 * @brief 3D Cartesian Kalman-filtered predictive aiming system.
 */
#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp>

struct AimCommand
{
    double pan_angle;  // Command for Turret
    double tilt_angle; // Command for Turret
    double pred_X;     // Predicted 3D X (mm)
    double pred_Y;     // Predicted 3D Y (mm)
    double pred_Z;     // Predicted 3D Z (mm)
    double pred_cx;    // Predicted 2D Pixel X (For UI)
    double pred_cy;    // Predicted 2D Pixel Y (For UI)
};

class PredictiveTurretSight
{
private:
    cv::KalmanFilter kf;
    int stateSize;
    int measSize;

    // Camera Intrinsic Parameters
    double Fx, Fy, Cx, Cy;
    double system_latency_sec;
    long last_timestamp_ms;

public:
    PredictiveTurretSight(double fx, double fy, double cx, double cy, double latency);

    /**
     * @brief Feeds a 3D measurement into the Kalman filter and predicts the future state.
     * @param measured_X Current X from triangulator (mm)
     * @param measured_Y Current Y from triangulator (mm)
     * @param measured_Z Current Z from triangulator (mm)
     * @param timestamp_ms Current system time in milliseconds
     */
    AimCommand updateAndPredict(double measured_X, double measured_Y, double measured_Z, long timestamp_ms);
};