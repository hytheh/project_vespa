/**
 * @file vespa_pipeline.cpp
 * @brief Implementation of the DeepStream Graph Construction.
 *
 * @section DESCRIPTION
 * Constructs the GStreamer pipeline string or element-by-element graph.
 * Configures the crucial `nvstreammux` batching and `nvinfer` properties.
 *
 * @section CRITICAL_CONFIG
 * - Sync: Enabled (relies on External Trigger Node ESP32-S2).
 * - Muxer Batch Size: 2 (Left + Right).
 * - MemType: NVBUF_MEM_CUDA_DEVICE (Zero-copy on Orin Nano).
 */

#include "vespa_pipeline.hpp"
#include "custom_probes.hpp"
#include <iostream>

VespaPipeline::VespaPipeline() : pipeline_(nullptr), loop_(nullptr), worker_ref_(nullptr) {
}

VespaPipeline::~VespaPipeline() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
    }
    if (loop_) {
        g_main_loop_unref(loop_);
    }
}

void VespaPipeline::init(int argc, char** argv, VespaWorker* worker) {
    worker_ref_ = worker;
    gst_init(&argc, &argv);
    loop_ = g_main_loop_new(NULL, FALSE);

    // 1. Create Elements
    pipeline_ = gst_pipeline_new("vespa-vision-pipeline");
    
    GstElement* source_left = gst_element_factory_make("v4l2src", "source_left");
    GstElement* source_right = gst_element_factory_make("v4l2src", "source_right");
    GstElement* stream_mux = gst_element_factory_make("nvstreammux", "stream_mux");
    GstElement* infer = gst_element_factory_make("nvinfer", "primary_inference");
    GstElement* tracker = gst_element_factory_make("nvtracker", "tracker");
    GstElement* sink = gst_element_factory_make("fakesink", "sink");

    if (!pipeline_ || !source_left || !source_right || !stream_mux || !infer || !tracker || !sink) {
        std::cerr << "[VespaPipeline] Failed to create elements." << std::endl;
        return;
    }

    // 2. Configure Elements
    // Sources
    g_object_set(G_OBJECT(source_left), "device", "/dev/video0", NULL);
    g_object_set(G_OBJECT(source_right), "device", "/dev/video1", NULL);

    // Muxer: Batch size 2 for stereo, CUDA memory for Jetson
    g_object_set(G_OBJECT(stream_mux), 
        "batch-size", 2, 
        "width", 1920, "height", 1080, 
        "batched-push-timeout", 40000, 
        "live-source", 1, 
        "nvbuf-memory-type", 0, // NVBUF_MEM_CUDA_DEVICE
        NULL);

    // Inference & Tracker Configs (Placeholders)
    g_object_set(G_OBJECT(infer), "config-file-path", "config_infer_primary.txt", NULL);
    g_object_set(G_OBJECT(tracker), "ll-config-file", "config_tracker.yml", NULL);

    // 3. Add to Pipeline
    gst_bin_add_many(GST_BIN(pipeline_), source_left, source_right, stream_mux, infer, tracker, sink, NULL);

    // 4. Link Elements
    // Link Sources to Muxer (Request Pads)
    GstPad* sinkpad_0 = gst_element_get_request_pad(stream_mux, "sink_0");
    GstPad* sinkpad_1 = gst_element_get_request_pad(stream_mux, "sink_1");
    GstPad* srcpad_left = gst_element_get_static_pad(source_left, "src");
    GstPad* srcpad_right = gst_element_get_static_pad(source_right, "src");

    if (gst_pad_link(srcpad_left, sinkpad_0) != GST_PAD_LINK_OK || 
        gst_pad_link(srcpad_right, sinkpad_1) != GST_PAD_LINK_OK) {
        std::cerr << "[VespaPipeline] Failed to link sources to muxer." << std::endl;
    }
    
    gst_object_unref(sinkpad_0);
    gst_object_unref(sinkpad_1);
    gst_object_unref(srcpad_left);
    gst_object_unref(srcpad_right);

    // Link Mux -> Infer -> Tracker -> Sink
    if (!gst_element_link_many(stream_mux, infer, tracker, sink, NULL)) {
        std::cerr << "[VespaPipeline] Failed to link main chain." << std::endl;
    }

    // 5. Attach Probe
    // Attaching to Tracker SRC pad to ensure object_id is available
    GstPad* tracker_src_pad = gst_element_get_static_pad(tracker, "src");
    gst_pad_add_probe(tracker_src_pad, GST_PAD_PROBE_TYPE_BUFFER, osd_sink_pad_buffer_probe, worker_ref_, NULL);
    gst_object_unref(tracker_src_pad);
}

void VespaPipeline::start() {
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
}

void VespaPipeline::run() {
    g_main_loop_run(loop_);
}