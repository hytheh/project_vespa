/** @file CameraInterface.h | @brief Handles V4L2 capture and NvBufSurface management. */
#ifndef VESPA_CAMERAINTERFACE_H
#define VESPA_CAMERAINTERFACE_H

namespace VESPA {
    class CameraInterface {
    public:
        CameraInterface();
        ~CameraInterface();
        void init();
        void tick(); // Main periodic execution
    };
}
#endif
