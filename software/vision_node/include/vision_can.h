/**
 * @file vision_can.h
 * @author Lead Software Comm Developer
 * @brief Linux SocketCAN wrapper for the Jetson Orin Nano (Vision Node).
 * @date 2026-03-16
 */

#ifndef VISION_CAN_H
#define VISION_CAN_H

#include <string>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "can_utils.h"

class VisionCan {
public:
    /**
     * @brief Construct a new Vision Can object.
     * @param interface_name Network interface name (e.g., "can0" or "can1").
     */
    VisionCan(const std::string& interface_name = "can0");

    /**
     * @brief Destructor. Closes the socket safely.
     */
    ~VisionCan();

    /**
     * @brief Initializes the SocketCAN interface and applies RX filters.
     * @return true if socket created and bound successfully, false otherwise.
     */
    bool begin();

    /**
     * @brief Transmits a common VESPA CAN frame.
     * @param frame Pointer to the frame to transmit.
     * @return true if successfully pushed to the kernel network stack.
     */
    bool sendFrame(const CAN_Frame_t* frame);

    /**
     * @brief Checks the socket queue and pulls a received frame if available.
     * Non-blocking operation.
     * @param frame Pointer to populate with received data.
     * @return true if a frame was read, false if queue is empty or error.
     */
    bool readFrame(CAN_Frame_t* frame);

private:
    std::string _interface_name;
    int _socket_fd;

    bool configureFilters();
    bool setNonBlocking();
};

#endif // VISION_CAN_H