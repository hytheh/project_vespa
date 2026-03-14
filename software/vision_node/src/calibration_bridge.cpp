/**
 * @file calibration_bridge.cpp
 * @brief Streams synchronized frames over ZMQ.
 */

#include "HardwareSyncController.h"
#include <iostream>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>
#include <unistd.h>
#include <limits.h>

/**
 * @brief Resolves the absolute path to the config file relative to this executable.
 * This completely removes the dependency on hardcoded usernames.
 */
std::string resolveConfigPath()
{
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1)
    {
        buffer[len] = '\0';
        std::string exePath(buffer);
        // Find the "build" directory in the path and replace everything after it
        size_t build_pos = exePath.find("/build/");
        if (build_pos != std::string::npos)
        {
            return exePath.substr(0, build_pos) + "/config/camera_settings.json";
        }
    }
    return "../config/camera_settings.json"; // Fallback
}

int main()
{
    std::cout << "=== VESPA CALIBRATION BRIDGE ===" << std::endl;

    // 1. Initialize ZMQ Publisher
    zmq::context_t context(1);
    zmq::socket_t publisher(context, ZMQ_PUB);
    publisher.bind("tcp://*:5555");

    // 2. Resolve user-independent config path
    std::string config_path = resolveConfigPath();
    std::cout << "[BRIDGE] Resolved config path: " << config_path << std::endl;

    // 3. Initialize Stereo Rig
    VESPA::HardwareSyncController stereoRig("/dev/video0", "/dev/video1", config_path);
    if (!stereoRig.initializeRig())
    {
        std::cerr << "[FATAL] Initialization failed. Exiting." << std::endl;
        return -1;
    }

    stereoRig.armTrigger();
    std::cout << "[BRIDGE] Streaming 60Hz synchronized pairs over ZMQ..." << std::endl;

    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 80};

    while (true)
    {
        VESPA::StereoPair pair = stereoRig.captureSynchronizedPair();
        if (pair.valid)
        {
            cv::Mat left_img(VESPA::SENSOR_HEIGHT, VESPA::SENSOR_WIDTH, CV_8UC1, pair.left_data);
            cv::Mat right_img(VESPA::SENSOR_HEIGHT, VESPA::SENSOR_WIDTH, CV_8UC1, pair.right_data);

            cv::Mat combined;
            cv::hconcat(left_img, right_img, combined);

            std::vector<uchar> buf;
            cv::imencode(".jpg", combined, buf, params);

            zmq::message_t msg(buf.size());
            memcpy(msg.data(), buf.data(), buf.size());
            publisher.send(msg, zmq::send_flags::none);

            stereoRig.releaseSynchronizedPair(pair);
        }
    }

    return 0;
}