#include "HardwareSyncController.h"

int main()
{
    VESPA::HardwareSyncController stereoRig("/dev/video0", "/dev/video1");

    if (!stereoRig.initializeRig())
    {
        return -1;
    }

    stereoRig.armTrigger();

    double left_ts, right_ts;

    for (int i = 0; i < 50; i++)
    {
        if (stereoRig.captureSynchronizedPair(left_ts, right_ts))
        {
            double offset = std::abs(left_ts - right_ts);
            std::cout << "Pair " << i << " grabbed! Hardware Drift: " << offset << " ms" << std::endl;
        }
        else
        {
            break; // Watchdog caught an error, trigger is already safely disarmed
        }
    }

    return 0;
}