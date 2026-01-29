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

#include "custom_probes.hpp"
#include "vespa_worker.hpp"
#include <gstnvdsmeta.h>
#include <nvdsmeta.h>

// Define the target class ID (Assuming 0 for Vespa Velutina based on training)
#define VESPA_VELUTINA 0

GstPadProbeReturn osd_sink_pad_buffer_probe(GstPad* pad, GstPadProbeInfo* info, gpointer user_data) {
    VespaWorker* worker = (VespaWorker*)user_data;
    GstBuffer* buf = (GstBuffer*)info->data;

    NvDsBatchMeta* batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta) {
        return GST_PAD_PROBE_OK;
    }

    // Iterate through frames in the batch
    for (NvDsMetaList* l_frame = batch_meta->frame_meta_list; l_frame != NULL; l_frame = l_frame->next) {
        NvDsFrameMeta* frame_meta = (NvDsFrameMeta*)(l_frame->data);

        // Iterate through objects in the frame
        for (NvDsMetaList* l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next) {
            NvDsObjectMeta* obj_meta = (NvDsObjectMeta*)(l_obj->data);

            // Filter: Ignore objects if class_id != VESPA_VELUTINA
            if (obj_meta->class_id != VESPA_VELUTINA) continue;

            // Extract & Populate VespaWorkItem
            VespaWorkItem item;
            item.x = obj_meta->rect_params.left;
            item.y = obj_meta->rect_params.top;
            item.w = obj_meta->rect_params.width;
            item.h = obj_meta->rect_params.height;
            item.object_id = obj_meta->object_id;
            item.confidence = obj_meta->confidence;

            // Push to worker
            worker->push(item);
        }
    }

    return GST_PAD_PROBE_OK;
}