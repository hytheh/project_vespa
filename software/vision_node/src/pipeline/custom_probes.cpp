/**
 * @file custom_probes.cpp
 * @brief GStreamer Pad Probes for Metadata Extraction and Logic Triggering.
 *
 * @section DESCRIPTION
 * This file contains the callback functions attached to GStreamer Pads.
 * It is the "Glue" between the DeepStream Metadata (Detection) and the 
 * Custom Algorithms (Depth/Ballistics).
 *
 * @section LOGIC
 * 1. `osd_sink_pad_buffer_probe`: 
 * - Iterates through NvDsFrameMeta.
 * - Extracts Bounding Box from Source 0 (Left).
 * - Triggers `VespaDepth::compute()` on Source 1 (Right) crop.
 * - Triggers `VespaBallistics::update()`.
 * - Triggers `VespaCAN::send()`.
 */