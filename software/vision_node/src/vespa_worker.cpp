/**
 * @file vespa_worker.cpp
 * @brief Implementation of the Asynchronous Worker.
 */

#include "vespa_worker.hpp"
#include <iostream>

VespaWorker::VespaWorker() : running_(false) {
}

VespaWorker::~VespaWorker() {
    stop();
}

void VespaWorker::start() {
    if (running_.load()) {
        return;
    }
    running_.store(true);
    worker_thread_ = std::thread(&VespaWorker::run, this);
    std::cout << "[VespaWorker] Thread started." << std::endl;
}

void VespaWorker::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    cv_.notify_all(); // Wake up the thread so it can exit

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    std::cout << "[VespaWorker] Thread stopped." << std::endl;
}

void VespaWorker::push(const VespaWorkItem& item) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(item);
    }
    cv_.notify_one();
}

void VespaWorker::run() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // Wait until we have work OR we are told to stop
        cv_.wait(lock, [this] { return !queue_.empty() || !running_.load(); });

        // Exit condition: Stopped and no more work
        if (!running_.load() && queue_.empty()) {
            break;
        }

        VespaWorkItem item = queue_.front();
        queue_.pop();
        
        // Unlock before processing to allow the producer to push more items
        lock.unlock();
        
        process_item(item);
    }
}

void VespaWorker::process_item(const VespaWorkItem& item) {
    // Placeholder for Depth -> Ballistics -> CAN logic
    std::cout << "[VespaWorker] Processing Target ID: " << item.object_id 
              << " | Conf: " << item.confidence << std::endl;
}