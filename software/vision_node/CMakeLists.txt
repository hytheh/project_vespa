# @file CMakeLists.txt
# @brief Master Build Configuration for the Vision Node (Jetson Orin Nano).
#
# @section DEPENDENCIES
# - CUDA 12.x (for VPI and TensorRT)
# - DeepStream 7.1 (GStreamer 1.0)
# - VPI 3.x (Vision Programming Interface)
# - SocketCAN (Linux Kernel)
#
# @section OUTPUT
# Generates the executable `vespa_vision_node` in the `build/bin` directory.
# Links against customized libraries for Pipeline, Depth, and Ballistics.

cmake_minimum_required(VERSION 3.10)
project(vespa_vision_node LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
# GStreamer (The engine)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)

# CUDA (The accelerator)
find_package(CUDAToolkit REQUIRED)

# --- Include Directories ---
include_directories(
    include
    ${GST_INCLUDE_DIRS}
    ${GST_APP_INCLUDE_DIRS}
    ${CUDAToolkit_INCLUDE_DIRS}
)

# --- Source Files (We will add more as we build) ---
set(SOURCES
    tests/cam_verification.cpp
)

# --- Executable: Camera Test ---
add_executable(vespa_cam_test ${SOURCES})

# Link Libraries
target_link_libraries(vespa_cam_test
    ${GST_LIBRARIES}
    ${GST_APP_LIBRARIES}
    CUDA::cudart
)