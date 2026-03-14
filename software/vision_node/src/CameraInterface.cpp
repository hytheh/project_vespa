#include "CameraInterface.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <cerrno>
#include <linux/videodev2.h>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <opencv2/core.hpp>

#define V4L2_CID_ARDUCAM_TRIGGER_MODE 0x00981901
#define V4L2_CID_ARDUCAM_EXPOSURE 0x00980911

namespace VESPA
{
    CameraInterface::CameraInterface(const std::string &devicePath)
        : m_devicePath(devicePath), m_fd(-1)
    {
        // PWM control removed. Orchestrator handles it now.
    }

    CameraInterface::~CameraInterface()
    {
        stopStream();
        if (m_fd >= 0)
            close(m_fd);
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
            std::cerr << "\033[1;31m[HAL WARNING]\033[0m Video Node rejected " << name << std::endl;
        }

        std::string subdevPath = findSubDevice(m_devicePath);
        int subFd = open(subdevPath.c_str(), O_RDWR);
        if (subFd >= 0)
        {
            if (ioctl(subFd, VIDIOC_S_CTRL, &ctrl) == 0)
            {
                std::cout << "[HAL] Subdevice successfully set " << name << " to " << value << std::endl;
            }
            close(subFd);
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

    bool CameraInterface::init()
    {
        checkRootPermissions();

        std::cout << "[HAL] Opening Camera: " << m_devicePath << std::endl;
        m_fd = open(m_devicePath.c_str(), O_RDWR);
        if (m_fd < 0)
        {
            std::cerr << "Failed to open camera device: " << m_devicePath << std::endl;
            return false;
        }

        struct v4l2_format fmt = {0};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = SENSOR_WIDTH;
        fmt.fmt.pix.height = SENSOR_HEIGHT;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
        if (ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0)
        {
            std::cerr << "Failed to set Pixel Format Y8" << std::endl;
            return false;
        }

        struct v4l2_requestbuffers req = {0};
        req.count = BUFFER_COUNT;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0)
        {
            std::cerr << "Failed to request buffers" << std::endl;
            return false;
        }

        for (int i = 0; i < BUFFER_COUNT; ++i)
        {
            struct v4l2_buffer buf = {0};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(m_fd, VIDIOC_QUERYBUF, &buf);
            m_buffers[i].length = buf.length;
            m_buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
        }
        return true;
    }

    void CameraInterface::loadSettings(const std::string &configPath, const std::string &cameraKey)
    {
        try
        {
            cv::FileStorage fs(configPath, cv::FileStorage::READ);
            if (!fs.isOpened())
                return;

            cv::FileNode cam = fs[cameraKey];
            m_settings.exposure = (int)cam["exposure"];
            m_settings.gain = (int)cam["analogue_gain"];
            m_settings.h_flip = (int)cam["horizontal_flip"];
            m_settings.v_flip = (int)cam["vertical_flip"];
        }
        catch (...)
        {
        }
    }

    void CameraInterface::startStream()
    {
        for (int i = 0; i < BUFFER_COUNT; ++i)
        {
            struct v4l2_buffer buf = {0};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(m_fd, VIDIOC_QBUF, &buf);
        }

        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(m_fd, VIDIOC_STREAMON, &type);

        setControl(V4L2_CID_ARDUCAM_EXPOSURE, m_settings.exposure, "Exposure");
        setControl(0x009e0903, m_settings.gain, "Analogue Gain");
    }

    void CameraInterface::stopStream()
    {
        if (m_fd < 0)
            return;
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(m_fd, VIDIOC_STREAMOFF, &type);
    }

    bool CameraInterface::captureFrame(double &timestamp_ms, uint8_t *&out_buffer, int &out_index)
    {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        fd_set fds;
        struct timeval tv;
        int r;

        // POSIX fix: Retry select() if interrupted by a SIGCHLD signal from std::system()
        do
        {
            FD_ZERO(&fds);
            FD_SET(m_fd, &fds);

            // Re-initialize timeout inside the loop because Linux select() modifies tv
            tv.tv_sec = 0;
            tv.tv_usec = 200000; // 200 ms timeout

            r = select(m_fd + 1, &fds, NULL, NULL, &tv);
        } while (r == -1 && errno == EINTR);

        if (r <= 0)
        {
            return false;
        }

        if (ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0)
        {
            return false;
        }

        timestamp_ms = (buf.timestamp.tv_sec * 1000.0) + (buf.timestamp.tv_usec / 1000.0);

        // Expose the zero-copy pointer and index to the caller
        out_buffer = static_cast<uint8_t *>(m_buffers[buf.index].start);
        out_index = buf.index;

        // CRITICAL CHANGE: We do NOT requeue the buffer here.
        return true;
    }

    void CameraInterface::releaseFrame(int index)
    {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = index;

        if (ioctl(m_fd, VIDIOC_QBUF, &buf) < 0)
        {
            std::cerr << "[HAL ERROR] Failed to release buffer " << index << std::endl;
        }
    }

    void CameraInterface::flushQueue()
    {
        int flags = fcntl(m_fd, F_GETFL, 0);
        fcntl(m_fd, F_SETFL, flags | O_NONBLOCK); // Set to non-blocking

        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        int flushed = 0;
        // Keep pulling until the queue is empty
        while (ioctl(m_fd, VIDIOC_DQBUF, &buf) == 0)
        {
            ioctl(m_fd, VIDIOC_QBUF, &buf);
            flushed++;
        }

        fcntl(m_fd, F_SETFL, flags); // Restore blocking mode
        std::cout << "[HAL] Flushed " << flushed << " stale frames from " << m_devicePath << std::endl;
    }
}