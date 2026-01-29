/**
 * @file vespa_pipeline.hpp
 * @brief Header for the GStreamer/DeepStream Pipeline Manager.
 *
 * @section DESCRIPTION
 * Defines the `VespaPipeline` class which encapsulates the DeepStream graph.
 * Manages the lifecycle of the GST Elements: Sources, Muxer, Inference, Tracker.
 *
 * @section ARCHITECTURE
 * - Source: 2x v4l2src (OV9281 Mono Global Shutter).
 * - Inference: nvinfer (YOLOv5/v8 - VespAI).
 * - Optimization: "Cyclops" logic (Inference on Left Stream only).
 */