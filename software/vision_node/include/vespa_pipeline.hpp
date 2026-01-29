/**
 * @file vespa_pipeline.hpp
 * @brief Manages the GStreamer/DeepStream lifecycle and connects elements.
 *
 * @section DESCRIPTION
 * This class encapsulates the GStreamer pipeline logic. It is responsible for
 * initializing the graph, linking elements (Source -> Mux -> Infer -> Tracker),
 * and managing the main loop.
 *
 * @section DEPENDENCIES
 * - GStreamer 1.0
 * - DeepStream SDK (nvstreammux, nvinfer, nvtracker)
 * - VespaWorker (for asynchronous processing)
 */

#ifndef VESPA_PIPELINE_HPP
#define VESPA_PIPELINE_HPP

#include <gst/gst.h>
#include "vespa_worker.hpp"

class VespaPipeline {
public:
    VespaPipeline();
    ~VespaPipeline();

    /**
     * @brief Initializes the pipeline, creates elements, and links them.
     */
    void init(int argc, char** argv, VespaWorker* worker);

    void start();
    void run();

private:
    GstElement* pipeline_;
    GMainLoop* loop_;
    VespaWorker* worker_ref_;
};

#endif // VESPA_PIPELINE_HPP