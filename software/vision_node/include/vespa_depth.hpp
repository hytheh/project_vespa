/**
 * @file vespa_depth.hpp
 * @brief Interface for VPI (Vision Programming Interface) Stereo Depth.
 *
 * @section DESCRIPTION
 * Defines the `VespaDepth` class responsible for GPU-accelerated Disparity estimation.
 *
 * @section STRATEGY ("Cyclops")
 * Instead of computing a full depth map, this class:
 * 1. Accepts a ROI (Region of Interest) based on the Left Camera Detection.
 * 2. Crops the corresponding region from the Right Camera image.
 * 3. Computes Dense Disparity only on that crop using VPI.
 *
 * @note Requires precise Camera Intrinsics/Extrinsics calibration data.
 */