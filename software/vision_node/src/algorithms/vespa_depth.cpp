/**
 * @file vespa_depth.cpp
 * @brief Implementation of VPI Stereo Logic.
 *
 * @section HARDWARE_ACCELERATION
 * - Target: NVIDIA PVA (Programmable Vision Accelerator) or CUDA.
 * - Input: NV12/GRAY8 via NvBufSurface.
 *
 * @section MATH
 * Z = (focal_length * baseline) / disparity
 */