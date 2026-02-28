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

    struct CameraSettings
    {
        int exposure = 681;
        int gain = 100;
        int h_flip = 0;
        int v_flip = 0;
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

        void loadSettings(const std::string &configPath, const std::string &cameraKey);

        // New Orchestration Methods
        void enablePWM();
        void disablePWM();

    private:
        std::string m_devicePath;
        int m_fd;
        FrameBuffer m_buffers[BUFFER_COUNT];
        CameraSettings m_settings;

        std::string findSubDevice(const std::string &videoDevice);
        void setControl(uint32_t id, int value, const std::string &name);
        void checkRootPermissions();
    };
}
#endif