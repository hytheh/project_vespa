/**
 * @file vespa_can.hpp
 * @brief SocketCAN Wrapper for V.E.S.P.A. Vision Node
 * @version 0.1
 */

#ifndef VESPA_CAN_HPP
#define VESPA_CAN_HPP

#include <string>
#include <mutex>
#include <atomic>
#include <linux/can.h>
#include "can_protocol.h"

/**
 * @class VespaCAN
 * @brief Handles low-level SocketCAN file descriptors and frame parsing.
 */
class VespaCAN {
public:
    VespaCAN(const std::string& interface_name);
    ~VespaCAN();

    /**
     * @brief Opens the socket and binds to the network interface.
     * @return true on success, false on failure.
     */
    bool init();

    /**
     * @brief Sends the calculated target data to the Motion Controller.
     * @param packet The populated target struct.
     */
    void send_target_packet(const VespaTargetPacket& packet);

    /**
     * @brief Sends a command to the Coordinator (e.g., Firing Permission).
     * @param cmd_id CAN ID (e.g., VESPA_CAN_ID_FIRING_CMD)
     * @param data Payload byte (1=Enable, 0=Disable)
     */
    void send_command(uint32_t cmd_id, uint8_t data);

private:
    std::string iface_name_;
    int sock_fd_;
    std::atomic<bool> is_connected_;

    // Helper for raw write
    void write_frame(const struct can_frame& frame);
};

#endif // VESPA_CAN_HPP