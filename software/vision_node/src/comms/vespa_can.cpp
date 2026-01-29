/**
 * @file vespa_can.cpp
 * @brief Implementation of SocketCAN Wrapper
 */

#include "vespa_can.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

VespaCAN::VespaCAN(const std::string& interface_name) 
    : iface_name_(interface_name), sock_fd_(-1), is_connected_(false) {}

VespaCAN::~VespaCAN() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
    }
}

bool VespaCAN::init() {
    // Open Socket
    if ((sock_fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("[VespaCAN] Error while opening socket");
        return false;
    }

    struct ifreq ifr;
    std::strcpy(ifr.ifr_name, iface_name_.c_str());
    ioctl(sock_fd_, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // Bind
    if (bind(sock_fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[VespaCAN] Error in socket bind");
        return false;
    }

    is_connected_ = true;
    std::cout << "[VespaCAN] Initialized on " << iface_name_ << std::endl;
    return true;
}

void VespaCAN::send_target_packet(const VespaTargetPacket& packet) {
    if (!is_connected_) return;

    struct can_frame frame;
    frame.can_id = VESPA_CAN_ID_TARGET_DATA;
    frame.can_dlc = sizeof(VespaTargetPacket);
    
    // Safety check: Ensure struct size matches CAN limit (8 bytes)
    static_assert(sizeof(VespaTargetPacket) == 8, "VespaTargetPacket must be 8 bytes");

    std::memcpy(frame.data, &packet, sizeof(VespaTargetPacket));
    
    write_frame(frame);
}

void VespaCAN::send_command(uint32_t cmd_id, uint8_t data) {
    if (!is_connected_) return;

    struct can_frame frame;
    frame.can_id = cmd_id;
    frame.can_dlc = 1;
    frame.data[0] = data;

    write_frame(frame);
}

void VespaCAN::write_frame(const struct can_frame& frame) {
    if (write(sock_fd_, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("[VespaCAN] Write error");
    }
}