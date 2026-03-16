/**
 * @file PredictiveTurretSight.h
 * @brief Kalman-filtered predictive aiming system for tracking and intercepting moving targets.
 */
#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp>

struct AimCommand
{
    double pan_angle;
    double tilt_angle;
};

class PredictiveTurretSight
{
private:
    cv::KalmanFilter kf;
    int stateSize;
    int measSize;

    // Camera Intrinsic Parameters
    double Fx, Fy, Cx, Cy;

    // System Latency (Seconds) for Lead Calculation
    double system_latency_sec;

    long last_timestamp_ms;

public:
    /**
     * @brief Initializes the predictive tracker with camera parameters and expected latency.
     */
    PredictiveTurretSight(double fx, double fy, double cx, double cy, double latency);

    /**
     * @brief Feeds a new detection into the Kalman filter and predicts the future intercept angle.
     * @param target_u X pixel coordinate of target
     * @param target_v Y pixel coordinate of target
     * @param current_turret_pan Current physical pan angle of the turret (in Radians)
     * @param current_turret_tilt Current physical tilt angle of the turret (in Radians)
     * @param timestamp_ms Current system time in milliseconds
     * @return AimCommand containing the predicted Pan and Tilt angles to command the servos
     */
    AimCommand updateAndPredict(double target_u, double target_v,
                                double current_turret_pan, double current_turret_tilt,
                                long timestamp_ms);
};