#include "HardwareSyncController.h"

namespace VESPA
{

    HardwareSyncController::HardwareSyncController(const std::string &leftDevice, const std::string &rightDevice)
        : m_leftDevName(leftDevice), m_rightDevName(rightDevice), m_isArmed(false)
    {
        // 1. Export PWM pin and clamp it instantly
        std::system("echo 0 > /sys/class/pwm/pwmchip3/export 2>/dev/null");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::system("echo 16666666 > /sys/class/pwm/pwmchip3/pwm0/period");
        disarmTrigger();

        m_leftCam = new CameraInterface(m_leftDevName);
        m_rightCam = new CameraInterface(m_rightDevName);
    }

    HardwareSyncController::~HardwareSyncController()
    {
        disarmTrigger();
        delete m_leftCam;
        delete m_rightCam;
    }

    bool HardwareSyncController::initializeRig()
    {
        std::cout << "[SYNC CONTROLLER] Initializing Stereo Rig..." << std::endl;

        // Will now correctly evaluate to boolean true/false
        if (!m_leftCam->init() || !m_rightCam->init())
        {
            std::cerr << "[FATAL] Failed to open video nodes." << std::endl;
            return false;
        }

        m_leftCam->startStream();
        m_rightCam->startStream();

        std::cout << "[SYNC CONTROLLER] Streams active. Waiting for Tegra driver to settle..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        std::cout << "[SYNC CONTROLLER] Forcing Left Camera to Slave Mode..." << std::endl;
        if (!forceSlaveMode(m_leftDevName))
            return false;

        std::cout << "[SYNC CONTROLLER] Forcing Right Camera to Slave Mode..." << std::endl;
        if (!forceSlaveMode(m_rightDevName))
            return false;

        m_leftCam->flushQueue();
        m_rightCam->flushQueue();

        std::cout << "\033[1;32m[SYNC CONTROLLER] Bus Contention Cleared. Rig is Ready.\033[0m" << std::endl;
        return true;
    }

    bool HardwareSyncController::forceSlaveMode(const std::string &deviceNode)
    {
        std::string cmd = "v4l2-ctl -d " + deviceNode +
                          " -c override_capture_timeout_ms=30000" +
                          " -c disable_frame_timeout=1" +
                          " -c frame_timeout=12000" +
                          " -c trigger_mode=1";
        int ret = std::system(cmd.c_str());
        return (ret == 0);
    }

    void HardwareSyncController::armTrigger()
    {
        if (!m_isArmed)
        {
            std::cout << "[SYNC CONTROLLER] Arming 60Hz Hardware Trigger..." << std::endl;
            std::system("echo 1666666 > /sys/class/pwm/pwmchip3/pwm0/duty_cycle");
            std::system("echo 1 > /sys/class/pwm/pwmchip3/pwm0/enable");
            m_isArmed = true;
        }
    }

    void HardwareSyncController::disarmTrigger()
    {
        std::cout << "[SYNC CONTROLLER] Disarming Trigger." << std::endl;
        std::system("echo 0 > /sys/class/pwm/pwmchip3/pwm0/duty_cycle");
        std::system("echo 0 > /sys/class/pwm/pwmchip3/pwm0/enable");
        m_isArmed = false;
    }

    bool HardwareSyncController::captureSynchronizedPair(double &left_ts_ms, double &right_ts_ms)
    {
        if (!m_leftCam->captureFrame("", left_ts_ms))
            return false;
        if (!m_rightCam->captureFrame("", right_ts_ms))
            return false;
        return true;
    }

} // End namespace VESPA