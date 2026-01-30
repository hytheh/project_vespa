/**
 * @file vespa_pipeline.cpp
 * @brief Fixed "Split-Brain" Implementation.
 * * @section TOPOLOGY
 * [Cam L] -> Tee -> Queue -> [Mux Infer (B1)] -> [Infer] -> [Tracker] -> [Sink]
 * |
 * v
 * Queue -> [Mux Depth (B2)] -> [Sink (Future Depth Probe)]
 * [Cam R] -> Queue ->       ^
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

    // 1. Create Pipeline
    pipeline_ = gst_pipeline_new("vespa-vision-pipeline");

    // 2. Create Elements
    // Sources
    GstElement* source_left = gst_element_factory_make("nvarguscamerasrc", "source_left");
    GstElement* source_right = gst_element_factory_make("nvarguscamerasrc", "source_right");

    // Splitter & Buffers
    GstElement* tee_left = gst_element_factory_make("tee", "tee_left");
    GstElement* queue_infer = gst_element_factory_make("queue", "queue_infer");
    GstElement* queue_depth_left = gst_element_factory_make("queue", "queue_depth_left");
    GstElement* queue_depth_right = gst_element_factory_make("queue", "queue_depth_right");

    // Muxers
    GstElement* mux_infer = gst_element_factory_make("nvstreammux", "mux_infer"); // Batch 1
    GstElement* mux_depth = gst_element_factory_make("nvstreammux", "mux_depth"); // Batch 2

    // Processing
    GstElement* infer = gst_element_factory_make("nvinfer", "primary_inference");
    GstElement* tracker = gst_element_factory_make("nvtracker", "tracker");
    
    // Sinks
    GstElement* sink_infer = gst_element_factory_make("fakesink", "sink_infer");
    GstElement* sink_depth = gst_element_factory_make("fakesink", "sink_depth");

    if (!pipeline_ || !source_left || !source_right || !tee_left || !mux_infer || !mux_depth) {
        std::cerr << "[VespaPipeline] Critical Error: Failed to create elements." << std::endl;
        return;
    }

    // 3. Configure Elements
    
    // Sources (Removed bufapi-version)
    g_object_set(G_OBJECT(source_left), "sensor-id", 0, NULL);
    g_object_set(G_OBJECT(source_right), "sensor-id", 1, NULL);

    // Mux Infer: BATCH SIZE 1 (Matches your Engine)
    g_object_set(G_OBJECT(mux_infer), 
        "batch-size", 1, 
        "width", 640, "height", 640, // Match Network Res to save VIC scaling
        "batched-push-timeout", 12500, 
        "live-source", 1, 
        "nvbuf-memory-type", 0, 
        NULL);

    // Mux Depth: BATCH SIZE 2 (For Stereo)
    g_object_set(G_OBJECT(mux_depth), 
        "batch-size", 2, 
        "width", 1280, "height", 800, // Keep Full Res for Depth
        "batched-push-timeout", 12500, 
        "live-source", 1, 
        "nvbuf-memory-type", 0, 
        NULL);

    // Inference & Tracker (Fixed Library Path)
    g_object_set(G_OBJECT(infer), "config-file-path", "deepstream_config/config_infer_primary.txt", NULL);
    g_object_set(G_OBJECT(tracker), 
        "ll-config-file", "deepstream_config/config_tracker.yml", 
        "ll-lib-file", "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
        NULL);

    // 4. Add to Pipeline Bin
    gst_bin_add_many(GST_BIN(pipeline_), 
        source_left, source_right, tee_left, 
        queue_infer, queue_depth_left, queue_depth_right,
        mux_infer, mux_depth, 
        infer, tracker, sink_infer, sink_depth, 
        NULL);

    // 5. Link Elements (The "Tee" Logic)

    // BRANCH A: Inference (Source Left -> Tee -> Queue -> MuxInfer -> Infer -> Tracker)
    // Link Source Left -> Tee
    gst_element_link(source_left, tee_left);

    // Link Tee -> Queue Infer -> Mux Infer (Sink 0)
    GstPad* tee_pad_infer = gst_element_get_request_pad(tee_left, "src_%u");
    GstPad* queue_infer_pad = gst_element_get_static_pad(queue_infer, "sink");
    gst_pad_link(tee_pad_infer, queue_infer_pad);
    gst_object_unref(tee_pad_infer); gst_object_unref(queue_infer_pad);

    GstPad* mux_infer_sink = gst_element_get_request_pad(mux_infer, "sink_0");
    GstPad* queue_infer_src = gst_element_get_static_pad(queue_infer, "src");
    gst_pad_link(queue_infer_src, mux_infer_sink);
    gst_object_unref(mux_infer_sink); gst_object_unref(queue_infer_src);

    gst_element_link_many(mux_infer, infer, tracker, sink_infer, NULL);

    // BRANCH B: Depth (Source Left + Source Right -> Mux Depth)
    
    // Path 1: Tee -> Queue Depth L -> Mux Depth (Sink 0)
    GstPad* tee_pad_depth = gst_element_get_request_pad(tee_left, "src_%u");
    GstPad* queue_depth_l_pad = gst_element_get_static_pad(queue_depth_left, "sink");
    gst_pad_link(tee_pad_depth, queue_depth_l_pad);
    gst_object_unref(tee_pad_depth); gst_object_unref(queue_depth_l_pad);

    GstPad* mux_depth_sink_0 = gst_element_get_request_pad(mux_depth, "sink_0");
    GstPad* queue_depth_l_src = gst_element_get_static_pad(queue_depth_left, "src");
    gst_pad_link(queue_depth_l_src, mux_depth_sink_0);
    gst_object_unref(mux_depth_sink_0); gst_object_unref(queue_depth_l_src);

    // Path 2: Source Right -> Queue Depth R -> Mux Depth (Sink 1)
    gst_element_link(source_right, queue_depth_right);

    GstPad* mux_depth_sink_1 = gst_element_get_request_pad(mux_depth, "sink_1");
    GstPad* queue_depth_r_src = gst_element_get_static_pad(queue_depth_right, "src");
    gst_pad_link(queue_depth_r_src, mux_depth_sink_1);
    gst_object_unref(mux_depth_sink_1); gst_object_unref(queue_depth_r_src);
    
    gst_element_link(mux_depth, sink_depth);

    // 6. Attach Probe (To Tracker Output on Branch A)
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