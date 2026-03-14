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
    cv::Mat img(height, width, CV_8UC1, const_cast<uint8_t *>(buffer));

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;

    cv::aruco::detectMarkers(img, dictionary_, corners, ids, parameters_);

    // Process detected markers and discard IDs
    targets.reserve(corners.size());
    for (size_t i = 0; i < corners.size(); ++i)
    {
        const auto &c = corners[i];

        // Compute raw sideways centroid
        float raw_cx = (c[0].x + c[1].x + c[2].x + c[3].x) * 0.25f;
        float raw_cy = (c[0].y + c[1].y + c[2].y + c[3].y) * 0.25f;

        // THE ZERO-COST KINEMATIC ROTATION:
        // Translate the 1280x800 sideways coordinates into 800x1280 upright coordinates.
        // Counter-Clockwise 90 degrees: X' = Y, Y' = Width - X
        float upright_cx = raw_cy;
        float upright_cy = width - raw_cx;

        // Compute approximate area using shoelace formula for a quadrilateral
        float area = 0.5f * std::abs(
                                (c[0].x * c[1].y - c[1].x * c[0].y) +
                                (c[1].x * c[2].y - c[2].x * c[1].y) +
                                (c[2].x * c[3].y - c[3].x * c[2].y) +
                                (c[3].x * c[0].y - c[0].x * c[3].y));

        targets.push_back({upright_cx, upright_cy, area});

#ifdef DEBUG_MODE
        std::cout << "[DEBUG] Upright Target generated at (" << upright_cx << ", " << upright_cy << "), Area: " << area << "\n";
#endif
    }

    return targets;
}