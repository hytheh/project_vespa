/**
 * @file HardwareSyncController.h
 * @brief Orchestrator for dual Arducam OV9281 stereoscopic synchronization.
 * @author Project V.E.S.P.A. Team
 * @details Safely manages the 1.8V MIPI CSI trigger pins to prevent I2C bus
 * contention. Implements OS-level locks to prevent collisions with the Python Calibration Tool.
 */

#pragma once

#include "CameraInterface.h"
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

namespace VESPA
{
    /**
     * @struct StereoPair
     * @brief Holds the zero-copy pointers and timestamps for a synchronized frame pair.
     */
    struct StereoPair
    {
        bool valid = false;
        uint8_t *left_data = nullptr;
        uint8_t *right_data = nullptr;
        int left_index = -1;
        int right_index = -1;
        double left_timestamp_ms = 0.0;
        double right_timestamp_ms = 0.0;
    };

    /**
     * @class HardwareSyncController
     * @brief High-level state machine controlling dual-camera V4L2 lifecycles.
     */
    class HardwareSyncController
    {
    public:
        /**
         * @brief Constructs the Sync Controller.
         * @param leftDevice Path to the left camera node.
         * @param rightDevice Path to the right camera node.
         * @param configPath Path to the optical settings JSON file.
         */
        HardwareSyncController(const std::string &leftDevice, const std::string &rightDevice, const std::string &configPath);

        /**
         * @brief Safely disarms the hardware trigger, releases locks, and cleans up.
         */
        ~HardwareSyncController();

        /**
         * @brief Executes the atomic boot sequence.
         * @details Acquires the OS lock, opens V4L2 streams, drains garbage frames,
         * and forces both cameras into Slave Mode before arming.
         * @return true if initialization and locks succeed, false otherwise.
         */
        bool initializeRig();

        /**
         * @brief Arms the 60Hz 3.3V PWM heartbeat.
         */
        void armTrigger();

        /**
         * @brief Instantly kills the hardware trigger heartbeat.
         */
        void disarmTrigger();

        /**
         * @brief Pulls perfectly synchronized frames from DMA RAM and yields their pointers.
         * @warning The caller MUST call releaseSynchronizedPair() when processing is complete.
         * @return A StereoPair struct containing the payload pointers and metadata.
         */
        StereoPair captureSynchronizedPair();

        /**
         * @brief Releases both stereo buffers back to the V4L2 kernel queues.
         * @param pair The StereoPair struct yielded by captureSynchronizedPair().
         */
        void releaseSynchronizedPair(const StereoPair &pair);

    private:
        CameraInterface *m_leftCam;  ///< Pointer to the Left Camera Interface
        CameraInterface *m_rightCam; ///< Pointer to the Right Camera Interface

        std::string m_leftDevName;  ///< Left camera V4L2 node path
        std::string m_rightDevName; ///< Right camera V4L2 node path

        std::string m_configPath; ///< Stores the path to the exposure JSON

        bool m_isArmed; ///< State flag tracking the PWM heartbeat
        int m_lockFd;   ///< File descriptor for the OS-level hardware lock

        /**
         * @brief Injects v4l2-ctl commands to force a camera into Slave Mode.
         * @param deviceNode The target V4L2 device.
         * @return true if the system call succeeds, false otherwise.
         */
        bool forceSlaveMode(const std::string &deviceNode);

        /**
         * @brief Attempts to acquire an exclusive OS-level lock to prevent bus contention.
         * @return true if acquired, false if another process holds it.
         */
        bool acquireHardwareLock();
    };
}