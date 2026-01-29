/**
 * @file vespa_worker.hpp
 * @brief Asynchronous Worker Thread for Heavy Computation.
 *
 * @section PATTERN
 * Producer-Consumer. The Video Pipeline produces detections; 
 * this Worker consumes them to calculate Depth and Ballistics 
 * without stalling the video stream.
 */

#ifndef VESPA_WORKER_HPP
#define VESPA_WORKER_HPP

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "vespa_types.hpp"

/**
 * @class VespaWorker
 * @brief Offloads heavy math (Depth/Ballistics) to a dedicated thread.
 */
class VespaWorker {
public:
    VespaWorker();
    ~VespaWorker();

    /**
     * @brief Starts the worker thread if not already running.
     */
    void start();

    /**
     * @brief Stops the worker thread cleanly, processing remaining items or exiting.
     */
    void stop();

    /**
     * @brief Thread-safe push method for the Producer (Pipeline).
     * @param item The detection data to process.
     */
    void push(const VespaWorkItem& item);

private:
    void run();
    void process_item(const VespaWorkItem& item);

    std::atomic<bool> running_;
    std::thread worker_thread_;
    
    std::queue<VespaWorkItem> queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
};

#endif // VESPA_WORKER_HPP