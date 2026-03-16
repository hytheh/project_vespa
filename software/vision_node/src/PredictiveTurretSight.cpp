#include "PredictiveTurretSight.h"
#include <cmath>

PredictiveTurretSight::PredictiveTurretSight(double fx, double fy, double cx, double cy, double latency)
    : Fx(fx), Fy(fy), Cx(cx), Cy(cy), system_latency_sec(latency), stateSize(4), measSize(2), last_timestamp_ms(0)
{
    kf.init(stateSize, measSize, 0, CV_32F);

    // Transition Matrix (F): Constant Velocity Model
    cv::setIdentity(kf.transitionMatrix);

    // Measurement Matrix (H): We only measure Pan and Tilt directly
    kf.measurementMatrix = cv::Mat::zeros(measSize, stateSize, CV_32F);
    kf.measurementMatrix.at<float>(0, 0) = 1.0f;
    kf.measurementMatrix.at<float>(1, 2) = 1.0f;

    // Process Noise (Q): How much we trust the math vs target's erratic behavior
    cv::setIdentity(kf.processNoiseCov, cv::Scalar::all(1e-4));

    // Measurement Noise (R): How noisy your pixel detections are
    cv::setIdentity(kf.measurementNoiseCov, cv::Scalar::all(1e-2));

    // Error Covariance (P)
    cv::setIdentity(kf.errorCovPost, cv::Scalar::all(1));
}

AimCommand PredictiveTurretSight::updateAndPredict(double target_u, double target_v,
                                                   double current_turret_pan, double current_turret_tilt,
                                                   long timestamp_ms)
{
    // 1. Calculate Delta Time (dt) in seconds
    double dt = (last_timestamp_ms == 0) ? 0.016 : (timestamp_ms - last_timestamp_ms) / 1000.0;
    last_timestamp_ms = timestamp_ms;

    // Update Transition Matrix with dynamic dt
    kf.transitionMatrix.at<float>(0, 1) = dt;
    kf.transitionMatrix.at<float>(2, 3) = dt;

    // 2. Kalman PREDICT (Where we think it is right now based on last velocity)
    cv::Mat prediction = kf.predict();

    // 3. Convert Pixel to Camera Angle (Using standard pinhole model in Radians)
    double cam_pan_angle = atan((target_u - Cx) / Fx);
    double cam_tilt_angle = atan((target_v - Cy) / Fy);

    // 4. Calculate Global Target Angle
    double measured_global_pan = current_turret_pan + cam_pan_angle;
    double measured_global_tilt = current_turret_tilt + cam_tilt_angle;

    // 5. Kalman CORRECT (Feed the measurement in)
    cv::Mat measurement = cv::Mat::zeros(measSize, 1, CV_32F);
    measurement.at<float>(0) = measured_global_pan;
    measurement.at<float>(1) = measured_global_tilt;

    cv::Mat estimated = kf.correct(measurement);

    // Extract smoothed state
    double est_pan = estimated.at<float>(0);
    double est_pan_vel = estimated.at<float>(1);
    double est_tilt = estimated.at<float>(2);
    double est_tilt_vel = estimated.at<float>(3);

    // 6. CALCULATE THE LEAD (Predict future position based on system latency)
    AimCommand cmd;
    cmd.pan_angle = est_pan + (est_pan_vel * system_latency_sec);
    cmd.tilt_angle = est_tilt + (est_tilt_vel * system_latency_sec);

    return cmd;
}