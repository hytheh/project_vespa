#ifndef VESPA_CAMERA_INTERFACE_H
#define VESPA_CAMERA_INTERFACE_H

#include <string>
#include <linux/videodev2.h>
#include <cstdint>
#include <vector>

namespace VESPA
{
    constexpr int SENSOR_WIDTH = 1280;
    constexpr int SENSOR_HEIGHT = 800;
    constexpr int BUFFER_COUNT = 10;

    struct FrameBuffer
    {
        void *start;
        size_t length;
    };

    class CameraInterface
    {
    public:
        CameraInterface(const std::string &devicePath);
        ~CameraInterface();

        void init();
        void startStream();
        void stopStream();
        bool captureFrame(const std::string &filename);

        // New Orchestration Methods
        void enablePWM();
        void disablePWM();

    private:
        std::string m_devicePath;
        int m_fd;
        FrameBuffer m_buffers[BUFFER_COUNT];

        std::string findSubDevice(const std::string &videoDevice);
        void setControl(uint32_t id, int value, const std::string &name);
        void checkRootPermissions();
    };
}
#endif