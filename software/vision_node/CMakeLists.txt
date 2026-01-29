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
set(VESPA_COMMON_INC "${CMAKE_SOURCE_DIR}/../common/inc")

include_directories(
    include
    ${GST_INCLUDE_DIRS}
    ${GST_APP_INCLUDE_DIRS}
    ${CUDAToolkit_INCLUDE_DIRS}
    ${VESPA_COMMON_INC}
)

# --- Source Files (More will be added as we build) ---
set(VISION_SRC
    src/main.cpp
    src/comms/vespa_can.cpp
    src/vespa_worker.cpp
    )

# --- Executable: Vision node ---
add_executable(vespa_vision_node ${VISION_SRC})
target_link_libraries(vespa_vision_node PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GST_LIBRARIES}
    ${GST_APP_LIBRARIES}
    CUDA::cudart
    pthread
)

# --- TEST SUITE (Creates a separate executable specifically for validation) ---
add_executable(vespa_protocol_test 
    "${CMAKE_SOURCE_DIR}/../tests/test_protocol.cpp"
)
target_include_directories(vespa_protocol_test PRIVATE ${VESPA_COMMON_INC})