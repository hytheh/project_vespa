#pragma once

#include "CameraInterface.h"
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>

namespace VESPA
{

    class HardwareSyncController
    {
    public:
        HardwareSyncController(const std::string &leftDevice, const std::string &rightDevice);
        ~HardwareSyncController();

        // The atomic boot sequence that prevents bus contention
        bool initializeRig();

        // Starts the 60Hz heartbeat
        void armTrigger();

        // Kills the heartbeat instantly
        void disarmTrigger();

        // Pulls both frames from DMA RAM and returns their exact hardware timestamps
        bool captureSynchronizedPair(double &left_ts_ms, double &right_ts_ms);

    private:
        CameraInterface *m_leftCam;
        CameraInterface *m_rightCam;

        std::string m_leftDevName;
        std::string m_rightDevName;

        bool m_isArmed;

        // Helper to force the JetVariety driver into Slave mode via system call
        bool forceSlaveMode(const std::string &deviceNode);
    };
}
