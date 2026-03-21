#include "PredictiveTurretSight.h"
#include <cmath>

PredictiveTurretSight::PredictiveTurretSight(double fx, double fy, double cx, double cy, double latency)
    : Fx(fx), Fy(fy), Cx(cx), Cy(cy), system_latency_sec(latency), stateSize(6), measSize(3), last_timestamp_ms(0)
{
    kf.init(stateSize, measSize, 0, CV_32F);

    cv::setIdentity(kf.transitionMatrix);

    kf.measurementMatrix = cv::Mat::zeros(measSize, stateSize, CV_32F);
    kf.measurementMatrix.at<float>(0, 0) = 1.0f;
    kf.measurementMatrix.at<float>(1, 2) = 1.0f;
    kf.measurementMatrix.at<float>(2, 4) = 1.0f;

    // --- THE HIGH-AGILITY TUNING ---
    // Trust the camera measurements deeply (Low Measurement Noise)
    cv::setIdentity(kf.measurementNoiseCov, cv::Scalar::all(1e-3));

    // Process Noise: Low noise for position, HIGH noise for velocity
    // This allows the filter to instantly adapt to erratic hand movements
    kf.processNoiseCov = cv::Mat::zeros(6, 6, CV_32F);
    kf.processNoiseCov.at<float>(0, 0) = 1e-3f; // X
    kf.processNoiseCov.at<float>(1, 1) = 5.0f;  // Vx (Highly adaptable)
    kf.processNoiseCov.at<float>(2, 2) = 1e-3f; // Y
    kf.processNoiseCov.at<float>(3, 3) = 5.0f;  // Vy (Highly adaptable)
    kf.processNoiseCov.at<float>(4, 4) = 1e-3f; // Z
    kf.processNoiseCov.at<float>(5, 5) = 5.0f;  // Vz (Highly adaptable)

    cv::setIdentity(kf.errorCovPost, cv::Scalar::all(1));
}

AimCommand PredictiveTurretSight::updateAndPredict(double measured_X, double measured_Y, double measured_Z, long timestamp_ms)
{
    double dt = (last_timestamp_ms == 0) ? 0.016 : (timestamp_ms - last_timestamp_ms) / 1000.0;

    // --- DROPOUT RESET ---
    // If the target is lost for more than half a second, reset the filter state
    // so we don't get a massive, incorrect velocity spike when it reappears.
    if (dt > 0.5)
    {
        kf.statePost.at<float>(0) = measured_X;
        kf.statePost.at<float>(1) = 0.0f;
        kf.statePost.at<float>(2) = measured_Y;
        kf.statePost.at<float>(3) = 0.0f;
        kf.statePost.at<float>(4) = measured_Z;
        kf.statePost.at<float>(5) = 0.0f;
        dt = 0.016;
    }
    last_timestamp_ms = timestamp_ms;

    kf.transitionMatrix.at<float>(0, 1) = dt;
    kf.transitionMatrix.at<float>(2, 3) = dt;
    kf.transitionMatrix.at<float>(4, 5) = dt;

    cv::Mat prediction = kf.predict();

    cv::Mat measurement = (cv::Mat_<float>(3, 1) << measured_X, measured_Y, measured_Z);
    cv::Mat estimated = kf.correct(measurement);

    double est_X = estimated.at<float>(0);
    double est_Vx = estimated.at<float>(1);
    double est_Y = estimated.at<float>(2);
    double est_Vy = estimated.at<float>(3);
    double est_Z = estimated.at<float>(4);
    double est_Vz = estimated.at<float>(5);

    AimCommand cmd;
    cmd.pred_X = est_X + (est_Vx * system_latency_sec);
    cmd.pred_Y = est_Y + (est_Vy * system_latency_sec);
    cmd.pred_Z = est_Z + (est_Vz * system_latency_sec);

    if (cmd.pred_Z > 1.0)
    {
        cmd.pred_cx = (cmd.pred_X * Fx / cmd.pred_Z) + Cx;
        cmd.pred_cy = (cmd.pred_Y * Fy / cmd.pred_Z) + Cy;

        cmd.pan_angle = atan(cmd.pred_X / cmd.pred_Z);
        cmd.tilt_angle = atan(cmd.pred_Y / cmd.pred_Z);
    }
    else
    {
        cmd.pred_cx = Cx;
        cmd.pred_cy = Cy;
        cmd.pan_angle = 0.0;
        cmd.tilt_angle = 0.0;
    }

    return cmd;
}