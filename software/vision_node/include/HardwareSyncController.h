/**
 * @file HardwareSyncController.h
 * @brief Orchestrator for dual Arducam OV9281 stereoscopic synchronization.
 * @author Project V.E.S.P.A. Team
 * @details Safely manages the 1.8V MIPI CSI trigger pins to prevent I2C bus
 * contention during the Master-to-Slave mode transition.
 */

#pragma once

#include "CameraInterface.h"
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>

namespace VESPA
{
    /**
     * @class HardwareSyncController
     * @brief High-level state machine controlling dual-camera V4L2 lifecycles.
     */
    class HardwareSyncController
    {
    public:
        /**
         * @brief Constructs the Sync Controller and instantly clamps the PWM pin.
         * @param leftDevice Path to the left camera node (e.g., "/dev/video0").
         * @param rightDevice Path to the right camera node (e.g., "/dev/video1").
         */
        HardwareSyncController(const std::string &leftDevice, const std::string &rightDevice);

        /**
         * @brief Safely disarms the hardware trigger and cleans up camera objects.
         */
        ~HardwareSyncController();

        /**
         * @brief Executes the atomic boot sequence.
         * @details Opens V4L2 streams, drains garbage frames, and forces both cameras
         * into Slave Mode before arming. Prevents bus contention.
         * @return true if initialization and slave-mode override succeed, false otherwise.
         */
        bool initializeRig();

        /**
         * @brief Arms the 60Hz 3.3V PWM heartbeat.
         * @note Must only be called after initializeRig() verifies high-impedance state.
         */
        void armTrigger();

        /**
         * @brief Instantly kills the hardware trigger heartbeat.
         */
        void disarmTrigger();

        /**
         * @brief Pulls perfectly synchronized frames from DMA RAM.
         * @param left_ts_ms Reference to store the exact hardware timestamp of the left frame.
         * @param right_ts_ms Reference to store the exact hardware timestamp of the right frame.
         * @return true if both frames were captured successfully, false if a watchdog timeout occurred.
         */
        bool captureSynchronizedPair(double &left_ts_ms, double &right_ts_ms);

    private:
        CameraInterface *m_leftCam;  ///< Pointer to the Left Camera Interface
        CameraInterface *m_rightCam; ///< Pointer to the Right Camera Interface

        std::string m_leftDevName;  ///< Left camera V4L2 node path
        std::string m_rightDevName; ///< Right camera V4L2 node path

        bool m_isArmed; ///< State flag tracking the PWM heartbeat

        /**
         * @brief Injects v4l2-ctl commands to force a camera into Slave Mode.
         * @param deviceNode The target V4L2 device.
         * @return true if the system call succeeds, false otherwise.
         */
        bool forceSlaveMode(const std::string &deviceNode);
    };
}