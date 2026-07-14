/**
 * @file sparse_tracker.cpp
 * @brief Zero-latency 3D triangulation using direct Hardware Sync.
 */
#include "HardwareSyncController.h"
#include "config_path.h"
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main()
{
    std::cout << "=== VESPA SPARSE 3D TRACKER ===" << std::endl;

    // 1. Load Calibration JSON
    std::string stereo_config = resolveConfigPath("stereovision_settings.json");
    std::ifstream file(stereo_config);
    if (!file.is_open())
    {
        std::cerr << "[ERROR] Could not find stereo JSON at: " << stereo_config << std::endl;
        return -1;
    }

    json calib;
    file >> calib;

    // Parse Matrices
    cv::Mat K_l = cv::Mat(3, 3, CV_64F);
    cv::Mat D_l = cv::Mat(1, 5, CV_64F);
    cv::Mat K_r = cv::Mat(3, 3, CV_64F);
    cv::Mat D_r = cv::Mat(1, 5, CV_64F);
    cv::Mat R = cv::Mat(3, 3, CV_64F);
    cv::Mat T = cv::Mat(3, 1, CV_64F);

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            K_l.at<double>(i, j) = calib["camera_matrix_left"][i][j];
            K_r.at<double>(i, j) = calib["camera_matrix_right"][i][j];
            R.at<double>(i, j) = calib["R"][i][j];
        }
        T.at<double>(i, 0) = calib["T"][i][0];
    }
    for (int i = 0; i < 5; i++)
    {
        D_l.at<double>(0, i) = calib["dist_coeff_left"][0][i];
        D_r.at<double>(0, i) = calib["dist_coeff_right"][0][i];
    }

    // 2. Compute Stereo Rectification Matrices
    cv::Mat R1, R2, P1, P2, Q;
    cv::Size img_size(1280, 800);
    cv::stereoRectify(K_l, D_l, K_r, D_r, img_size, R, T, R1, R2, P1, P2, Q);
    std::cout << "[TRACKER] Mathematical Triangulation Matrices Initialized." << std::endl;

    // 3. Setup ArUco Detector
    cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
    cv::Ptr<cv::aruco::DetectorParameters> parameters = cv::aruco::DetectorParameters::create();

    // 4. Initialize Hardware Rig Directly!
    std::string hardware_config = resolveConfigPath("camera_settings.json");
    VESPA::HardwareSyncController stereoRig("/dev/video0", "/dev/video1", hardware_config);

    if (!stereoRig.initializeRig())
    {
        std::cerr << "[FATAL] Failed to initialize hardware rig." << std::endl;
        return -1;
    }

    stereoRig.armTrigger();
    std::cout << "[TRACKER] Hardware Armed. Tracking live..." << std::endl;

    while (true)
    {
        // Grab perfectly synchronized pair directly from DMA RAM
        VESPA::StereoPair pair = stereoRig.captureSynchronizedPair();

        if (pair.valid)
        {
            // Map raw memory pointers to OpenCV matrices (Zero-Copy)
            cv::Mat img_left(800, 1280, CV_8UC1, pair.left_data);
            cv::Mat img_right(800, 1280, CV_8UC1, pair.right_data);

            // Detect Markers
            std::vector<int> ids_l, ids_r;
            std::vector<std::vector<cv::Point2f>> corners_l, corners_r;
            cv::aruco::detectMarkers(img_left, dictionary, corners_l, ids_l, parameters);
            cv::aruco::detectMarkers(img_right, dictionary, corners_r, ids_r, parameters);

            // Match IDs between Left and Right cameras
            for (size_t i = 0; i < ids_l.size(); i++)
            {
                int target_id = ids_l[i];
                auto it = std::find(ids_r.begin(), ids_r.end(), target_id);

                if (it != ids_r.end())
                {
                    size_t j = std::distance(ids_r.begin(), it);

                    // Calculate RAW pixel center
                    cv::Point2f center_l(0, 0), center_r(0, 0);
                    for (int k = 0; k < 4; k++)
                    {
                        center_l += corners_l[i][k];
                        center_r += corners_r[j][k];
                    }
                    center_l *= 0.25f;
                    center_r *= 0.25f;

                    // UNDISTORT the single point
                    std::vector<cv::Point2f> clean_l, clean_r;
                    cv::undistortPoints(std::vector<cv::Point2f>{center_l}, clean_l, K_l, D_l, R1, P1);
                    cv::undistortPoints(std::vector<cv::Point2f>{center_r}, clean_r, K_r, D_r, R2, P2);

                    // TRIANGULATE the 3D position
                    cv::Mat point_4D;
                    cv::triangulatePoints(P1, P2, clean_l, clean_r, point_4D);

                    // Convert homogeneous coordinates to mm
                    float X = point_4D.at<float>(0, 0) / point_4D.at<float>(3, 0);
                    float Y = point_4D.at<float>(1, 0) / point_4D.at<float>(3, 0);
                    float Z = point_4D.at<float>(2, 0) / point_4D.at<float>(3, 0);

                    std::cout << "\r[Target ID: " << target_id << "] X: " << std::fixed << std::setprecision(1) << X
                              << " mm | Y: " << Y << " mm | Depth (Z): " << Z << " mm      " << std::flush;
                }
            }

            // Release the memory back to the Tegra driver
            stereoRig.releaseSynchronizedPair(pair);
        }
    }
    return 0;
}