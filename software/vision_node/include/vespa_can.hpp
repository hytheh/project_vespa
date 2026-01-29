/**
 * @file vespa_can.hpp
 * @brief Wrapper for Linux SocketCAN interface.
 *
 * @section DESCRIPTION
 * Handles the raw socket creation, filtering, and frame transmission.
 * Provides a thread-safe API for the Pipeline Probe to send target data.
 *
 * @section SAFETY
 * - Non-blocking writes (to avoid stalling the Vision Pipeline).
 * - Error counting (reports if CAN bus is down).
 */