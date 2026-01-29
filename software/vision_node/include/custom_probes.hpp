/**
 * @file custom_probes.hpp
 * @brief Declaration of GStreamer Pad Probes.
 *
 * @section DESCRIPTION
 * Exposes the callback functions used to intercept GStreamer buffers.
 * These probes are attached to specific pads (e.g., Tracker Src) to extract
 * DeepStream metadata (NvDsBatchMeta) and pass it to the application logic.
 *
 * @section USAGE
 * - Linked in `VespaPipeline::init()`.
 * - Executed in the streaming thread context.
 */

#ifndef CUSTOM_PROBES_HPP
#define CUSTOM_PROBES_HPP

#include <gst/gst.h>

/**
 * @brief Probe attached to the Tracker to extract metadata and push to Worker.
 */
GstPadProbeReturn osd_sink_pad_buffer_probe(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);

#endif // CUSTOM_PROBES_HPP