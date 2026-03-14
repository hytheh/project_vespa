/**
 * @file HardwareSyncController.cpp
 * @brief Implementation of the Master/Slave synchronization orchestrator.
 */

#include "HardwareSyncController.h"

namespace VESPA
{

    HardwareSyncController::HardwareSyncController(const std::string &leftDevice, const std::string &rightDevice, const std::string &configPath)
        : m_leftDevName(leftDevice), m_rightDevName(rightDevice), m_configPath(configPath), m_isArmed(false), m_lockFd(-1)
    {
        m_leftCam = new CameraInterface(m_leftDevName);
        m_rightCam = new CameraInterface(m_rightDevName);
    }

    HardwareSyncController::~HardwareSyncController()
    {
        disarmTrigger();
        delete m_leftCam;
        delete m_rightCam;

        // Cleanly release the OS hardware lock so other tools can run
        if (m_lockFd >= 0)
        {
            flock(m_lockFd, LOCK_UN);
            close(m_lockFd);
            std::cout << "[SYNC CONTROLLER] Hardware lock released." << std::endl;
        }
    }

    bool HardwareSyncController::acquireHardwareLock()
    {
        // FIX: Added O_CLOEXEC to prevent the FD from leaking to v4l2-ctl child processes
        m_lockFd = open("/tmp/vespa_hardware.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0666);
        if (m_lockFd < 0)
        {
            std::cerr << "[FATAL] Could not create or open /tmp/vespa_hardware.lock" << std::endl;
            return false;
        }

        // Attempt an exclusive, non-blocking lock
        if (flock(m_lockFd, LOCK_EX | LOCK_NB) < 0)
        {
            std::cerr << "\n\033[1;31m[FATAL BUS CONTENTION HAZARD]\033[0m" << std::endl;
            std::cerr << "[FATAL] Hardware is currently locked by another process (Likely Python Calibration Tool)." << std::endl;
            std::cerr << "[FATAL] Aborting boot sequence immediately to prevent sensor burnout." << std::endl;
            close(m_lockFd);
            m_lockFd = -1;
            return false;
        }

        std::cout << "[SYNC CONTROLLER] Exclusive hardware lock acquired." << std::endl;
        return true;
    }

    bool HardwareSyncController::initializeRig()
    {
        std::cout << "[SYNC CONTROLLER] Initializing Stereo Rig..." << std::endl;

        // 1. MANDATORY SAFETY GATE: Acquire OS lock before touching ANY hardware
        if (!acquireHardwareLock())
        {
            return false;
        }

        // 2. NOW it is safe to clamp the PWM pin (Moved from constructor)
        std::system("echo 0 > /sys/class/pwm/pwmchip3/export 2>/dev/null");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // FIX 1: Call disarmTrigger() before setting the period to prevent sysfs rejection
        disarmTrigger();
        std::system("echo 16666666 > /sys/class/pwm/pwmchip3/pwm0/period 2>/dev/null");

        // 3. Initialize V4L2 Streams
        if (!m_leftCam->init() || !m_rightCam->init())
        {
            std::cerr << "[FATAL] Failed to open video nodes." << std::endl;
            return false;
        }

        // Start the hardware streams FIRST to wake up the PLL clocks
        m_leftCam->startStream();
        m_rightCam->startStream();

        // THE FIX: Let the hardware stabilize before sending I2C commands
        std::cout << "[SYNC CONTROLLER] Waiting 500ms for sensor clocks to stabilize..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Now load and apply the JSON settings to the awake, active registers
        std::cout << "[SYNC CONTROLLER] Loading optical settings from: " << m_configPath << std::endl;
        m_leftCam->loadSettings(m_configPath, "camera0");
        m_rightCam->loadSettings(m_configPath, "camera1");

        // 4. Force Slave Mode (CRITICAL: Overrides Python tool's Master Mode state)
        std::cout << "[SYNC CONTROLLER] Forcing Left Camera to Slave Mode..." << std::endl;
        if (!forceSlaveMode(m_leftDevName))
            return false;

        std::cout << "[SYNC CONTROLLER] Forcing Right Camera to Slave Mode..." << std::endl;
        if (!forceSlaveMode(m_rightDevName))
            return false;

        // FIX 2: Sleep for 50ms to allow in-flight Master Mode frames to finish writing to DMA RAM
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
        std::system("echo 0 > /sys/class/pwm/pwmchip3/pwm0/duty_cycle 2>/dev/null");
        std::system("echo 0 > /sys/class/pwm/pwmchip3/pwm0/enable 2>/dev/null");
        m_isArmed = false;
    }

    StereoPair HardwareSyncController::captureSynchronizedPair()
    {
        StereoPair pair;

        bool left_ok = m_leftCam->captureFrame(pair.left_timestamp_ms, pair.left_data, pair.left_index);
        bool right_ok = m_rightCam->captureFrame(pair.right_timestamp_ms, pair.right_data, pair.right_index);

        // Keep pulling frames from the V4L2 queues until their timestamps match perfectly
        while (left_ok && right_ok)
        {
            double diff = pair.left_timestamp_ms - pair.right_timestamp_ms;

            // If timestamps are within 5 milliseconds of each other, they are a perfect match
            if (std::abs(diff) <= 5.0)
            {
                pair.valid = true;
                return pair;
            }

            // Desync detected! Drop the older frame and pull a fresh one from that specific camera
            if (diff < 0)
            {
                // Left frame is older, throw it away
                m_leftCam->releaseFrame(pair.left_index);
                left_ok = m_leftCam->captureFrame(pair.left_timestamp_ms, pair.left_data, pair.left_index);
            }
            else
            {
                // Right frame is older, throw it away
                m_rightCam->releaseFrame(pair.right_index);
                right_ok = m_rightCam->captureFrame(pair.right_timestamp_ms, pair.right_data, pair.right_index);
            }
        }

        // If we break the loop, a camera failed to yield a frame
        if (left_ok)
            m_leftCam->releaseFrame(pair.left_index);
        if (right_ok)
            m_rightCam->releaseFrame(pair.right_index);
        pair.valid = false;

        return pair;
    }

    void HardwareSyncController::releaseSynchronizedPair(const StereoPair &pair)
    {
        if (pair.left_index >= 0)
            m_leftCam->releaseFrame(pair.left_index);
        if (pair.right_index >= 0)
            m_rightCam->releaseFrame(pair.right_index);
    }

} // End namespace VESPA