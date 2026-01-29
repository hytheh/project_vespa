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