#pragma once

#include <vector>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

struct Target2D
{
    float cx;
    float cy;
    float area;
};

class ArUcoTracker
{
public:
    ArUcoTracker();

    // Accepts raw 8-bit grayscale buffer. Width and height are expected to be 1280x800.
    std::vector<Target2D> detect(const uint8_t *buffer, int width, int height) const;

private:
    cv::Ptr<cv::aruco::Dictionary> dictionary_;
    cv::Ptr<cv::aruco::DetectorParameters> parameters_;
};