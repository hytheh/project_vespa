#include "CameraInterface.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <cerrno>
#include <linux/videodev2.h>
#include <thread>
#include <chrono>
#include <cstdlib> // For std::system

// Adjust these paths to match your actual script locations!
#define CMD_PWM_START "sudo /home/hytheh/project_vespa/software/vision_node/PWM_sync_start.sh"
#define CMD_PWM_STOP "sudo /home/hytheh/project_vespa/software/vision_node/PWM_sync_stop.sh"
// Note: You might need a specific 'stop' script that kills the generator process or sets GPIO to 0.

#define V4L2_CID_ARDUCAM_TRIGGER_MODE 0x00981901
#define V4L2_CID_ARDUCAM_DISABLE_TIMEOUT 0x00981902
#define V4L2_CID_ARDUCAM_FRAME_TIMEOUT 0x00981903
#define V4L2_CID_ARDUCAM_EXPOSURE 0x00980911

namespace VESPA
{
    CameraInterface::CameraInterface(const std::string &devicePath)
        : m_devicePath(devicePath), m_fd(-1)
    {
    }

    CameraInterface::~CameraInterface()
    {
        stopStream();
        disablePWM(); // Safety shutdown
        if (m_fd >= 0)
            close(m_fd);
    }

    void CameraInterface::enablePWM()
    {
        std::cout << "[HAL] Starting External PWM..." << std::endl;
        int ret = std::system(CMD_PWM_START);
        if (ret != 0)
            std::cerr << "[WARNING] Failed to start PWM script." << std::endl;
    }

    void CameraInterface::disablePWM()
    {
        // Silent shutdown to avoid log spam on init
        std::system(CMD_PWM_STOP " > /dev/null 2>&1");
    }

    void CameraInterface::checkRootPermissions()
    {
        if (geteuid() != 0)
        {
            std::cerr << "\033[1;31m[CRITICAL] Application is not running as root!\033[0m" << std::endl;
        }
    }

    void CameraInterface::setControl(uint32_t id, int value, const std::string &name)
    {
        struct v4l2_control ctrl = {0};
        ctrl.id = id;
        ctrl.value = value;
        if (ioctl(m_fd, VIDIOC_S_CTRL, &ctrl) < 0)
        {
            // Suppress noise
        }
    }

    std::string CameraInterface::findSubDevice(const std::string &videoDevice)
    {
        char indexChar = videoDevice.back();
        if (!isdigit(indexChar))
            indexChar = '0';
        std::string subdevPath = "/dev/v4l-subdev";
        subdevPath += indexChar;
        return subdevPath;
    }

    void CameraInterface::init()
    {
        checkRootPermissions();

        // 1. HARD STOP PWM (Clean Slate)
        disablePWM();

        std::cout << "[HAL] Opening Camera: " << m_devicePath << std::endl;
        m_fd = open(m_devicePath.c_str(), O_RDWR);
        if (m_fd < 0)
            throw std::runtime_error("Failed to open camera device");

        // Dummy subdev open (kept for completeness)
        int subFd = open(findSubDevice(m_devicePath).c_str(), O_RDWR);
        if (subFd >= 0)
            close(subFd);

        struct v4l2_format fmt = {0};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = SENSOR_WIDTH;
        fmt.fmt.pix.height = SENSOR_HEIGHT;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
        if (ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0)
            throw std::runtime_error("Failed to set Pixel Format Y8");

        struct v4l2_requestbuffers req = {0};
        req.count = BUFFER_COUNT;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0)
            throw std::runtime_error("Failed to request buffers");

        for (int i = 0; i < BUFFER_COUNT; ++i)
        {
            struct v4l2_buffer buf = {0};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0)
                throw std::runtime_error("VIDIOC_QUERYBUF failed");
            m_buffers[i].length = buf.length;
            m_buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
        }
    }

    void CameraInterface::startStream()
    {
        // 1. Queue Buffers
        for (int i = 0; i < BUFFER_COUNT; ++i)
        {
            struct v4l2_buffer buf = {0};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(m_fd, VIDIOC_QBUF, &buf);
        }

        // 2. Stream On (Starts in Master Mode)
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(m_fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("STREAMON failed");

        // 3. Wait for sensor init
        std::cout << "[HAL] Stream Started. Waiting 300ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 4. Configure Trigger (Forces Slave Mode)
        std::cout << "[HAL] Forcing Slave Mode..." << std::endl;
        setControl(V4L2_CID_ARDUCAM_DISABLE_TIMEOUT, 1, "Disable Frame Timeout");
        setControl(V4L2_CID_ARDUCAM_FRAME_TIMEOUT, 12000, "Frame Timeout");
        setControl(V4L2_CID_ARDUCAM_TRIGGER_MODE, 1, "Trigger Mode");
        setControl(V4L2_CID_ARDUCAM_EXPOSURE, 5000, "Exposure");

        // 5. Drain Junk (PWM is OFF, so this clears the Master Mode bursts)
        std::cout << "[HAL] Draining junk frames..." << std::endl;
        int flags = fcntl(m_fd, F_GETFL, 0);
        fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);

        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        int empty_strikes = 0;
        int flushed = 0;
        while (empty_strikes < 5)
        { // Should exit quickly because PWM is OFF
            if (ioctl(m_fd, VIDIOC_DQBUF, &buf) >= 0)
            {
                ioctl(m_fd, VIDIOC_QBUF, &buf);
                flushed++;
                empty_strikes = 0;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                empty_strikes++;
            }
        }
        fcntl(m_fd, F_SETFL, flags);
        std::cout << "[HAL] Drained " << flushed << " frames. Camera Armed." << std::endl;
    }

    void CameraInterface::stopStream()
    {
        if (m_fd >= 0)
            setControl(V4L2_CID_ARDUCAM_TRIGGER_MODE, 0, "Trigger Mode");
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(m_fd, VIDIOC_STREAMOFF, &type);
    }

    bool CameraInterface::captureFrame(const std::string &filename)
    {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        while (true)
        {
            // Block until trigger (or watchdog, but PWM will be ON, so trigger wins)
            if (ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0)
            {
                perror("VIDIOC_DQBUF");
                return false;
            }

            if (buf.bytesused == 0)
            {
                ioctl(m_fd, VIDIOC_QBUF, &buf);
                continue;
            }

            std::cout << "[HAL] Frame Captured! Bytes: " << buf.bytesused << std::endl;
            break;
        }

        std::ofstream file(filename, std::ios::binary);
        file << "P5\n"
             << SENSOR_WIDTH << " " << SENSOR_HEIGHT << "\n255\n";
        file.write((char *)m_buffers[buf.index].start, buf.bytesused);
        file.close();

        ioctl(m_fd, VIDIOC_QBUF, &buf);
        return true;
    }
}