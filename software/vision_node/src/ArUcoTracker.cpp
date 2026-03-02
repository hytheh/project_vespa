#include "ArUcoTracker.h"

ArUcoTracker::ArUcoTracker()
{
    // Initialize standard 4x4_50 dictionary
    dictionary_ = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
    parameters_ = cv::aruco::DetectorParameters::create();

    // Optimization for latency: Disable advanced subpixel refinement if not strictly needed
    parameters_->cornerRefinementMethod = cv::aruco::CORNER_REFINE_NONE;
}

std::vector<Target2D> ArUcoTracker::detect(const uint8_t *buffer, int width, int height) const
{
    std::vector<Target2D> targets;

    // Zero-copy wrapping of the raw pointer.
    // const_cast is required because cv::Mat expects a mutable pointer,
    // but the detectMarkers function will not modify the underlying image data.
    cv::Mat img(height, width, CV_8UC1, const_cast<uint8_t *>(buffer));

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;

    cv::aruco::detectMarkers(img, dictionary_, corners, ids, parameters_);

    // Process detected markers and discard IDs
    targets.reserve(corners.size());
    for (size_t i = 0; i < corners.size(); ++i)
    {
        const auto &c = corners[i];

        // Compute centroid (cx, cy)
        float cx = (c[0].x + c[1].x + c[2].x + c[3].x) * 0.25f;
        float cy = (c[0].y + c[1].y + c[2].y + c[3].y) * 0.25f;

        // Compute approximate area using shoelace formula for a quadrilateral
        float area = 0.5f * std::abs(
                                (c[0].x * c[1].y - c[1].x * c[0].y) +
                                (c[1].x * c[2].y - c[2].x * c[1].y) +
                                (c[2].x * c[3].y - c[3].x * c[2].y) +
                                (c[3].x * c[0].y - c[0].x * c[3].y));

        targets.push_back({cx, cy, area});

#ifdef DEBUG_MODE
        std::cout << "[DEBUG] Target detected at (" << cx << ", " << cy << "), Area: " << area << "\n";
#endif
    }

    return targets;
}