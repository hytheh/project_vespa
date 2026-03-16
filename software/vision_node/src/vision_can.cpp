/**
 * @file vision_can.cpp
 * @author Lead Software Comm Developer
 * @brief Implementation of the Jetson Orin Nano SocketCAN wrapper.
 * @date 2026-03-16
 */

#include "vision_can.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

VisionCan::VisionCan(const std::string& interface_name) 
    : _interface_name(interface_name), _socket_fd(-1) {}

VisionCan::~VisionCan() {
    if (_socket_fd >= 0) {
        close(_socket_fd);
    }
}

bool VisionCan::begin() {
    struct sockaddr_can addr;
    struct ifreq ifr;

    // 1. Create a RAW SocketCAN socket
    if ((_socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        std::cerr << "[VisionCan] Error: Could not create SocketCAN socket." << std::endl;
        return false;
    }

    // 2. Resolve the interface index by name (e.g., "can0")
    std::strncpy(ifr.ifr_name, _interface_name.c_str(), IFNAMSIZ - 1);
    if (ioctl(_socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "[VisionCan] Error: Could not resolve interface " << _interface_name << std::endl;
        close(_socket_fd);
        return false;
    }

    // 3. Bind the socket to the interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "[VisionCan] Error: Could not bind socket to " << _interface_name << std::endl;
        close(_socket_fd);
        return false;
    }

    // 4. Configure RX Filters & Non-Blocking mode
    if (!configureFilters() || !setNonBlocking()) {
        close(_socket_fd);
        return false;
    }

    return true;
}

bool VisionCan::configureFilters() {
    // The Vision Node only needs to subscribe to specific messages:
    // ERR_MOTION (0x030), ERR_COORD (0x040), HB_MOTION (0x110), HB_COORD (0x120)
    struct can_filter rfilter[4];

    rfilter[0].can_id   = CAN_ID_POS_MOTION;
    rfilter[0].can_mask = CAN_SFF_MASK;
    
    rfilter[1].can_id   = CAN_ID_POS_COORD;
    rfilter[1].can_mask = CAN_SFF_MASK;

    rfilter[2].can_id   = CAN_ID_HB_MOTION;
    rfilter[2].can_mask = CAN_SFF_MASK;

    rfilter[3].can_id   = CAN_ID_HB_COORD;
    rfilter[3].can_mask = CAN_SFF_MASK;

    if (setsockopt(_socket_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0) {
        std::cerr << "[VisionCan] Error: Failed to set socket filters." << std::endl;
        return false;
    }
    return true;
}

bool VisionCan::setNonBlocking() {
    int flags = fcntl(_socket_fd, F_GETFL, 0);
    if (flags == -1) return false;
    if (fcntl(_socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) return false;
    return true;
}

bool VisionCan::sendFrame(const CAN_Frame_t* frame) {
    if (_socket_fd < 0 || !frame) return false;

    struct can_frame linux_frame;
    linux_frame.can_id = frame->id;
    linux_frame.can_dlc = frame->dlc;
    std::memcpy(linux_frame.data, frame->data, frame->dlc);

    // Padding the rest of the payload with 0s is good practice for security/cleanliness
    if (frame->dlc < 8) {
        std::memset(linux_frame.data + frame->dlc, 0, 8 - frame->dlc);
    }

    if (write(_socket_fd, &linux_frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        return false;
    }
    return true;
}

bool VisionCan::readFrame(CAN_Frame_t* frame) {
    if (_socket_fd < 0 || !frame) return false;

    struct can_frame linux_frame;
    ssize_t nbytes = read(_socket_fd, &linux_frame, sizeof(struct can_frame));

    // If nothing is in the queue, read returns -1 with errno EAGAIN/EWOULDBLOCK
    if (nbytes < 0) {
        return false;
    }

    if (nbytes < (ssize_t)sizeof(struct can_frame)) {
        return false; // Incomplete read
    }

    frame->id = linux_frame.can_id & CAN_SFF_MASK; // Mask out standard frame format bits
    frame->dlc = linux_frame.can_dlc;
    std::memcpy(frame->data, linux_frame.data, frame->dlc);

    return true;
}