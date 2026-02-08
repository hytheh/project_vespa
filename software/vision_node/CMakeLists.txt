cmake_minimum_required(VERSION 3.14)
project(VisionNode VERSION 0.1.0 LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
# CUDA
find_package(CUDA REQUIRED)

# VPI (Vision Programming Interface) for Lens Distortion & Scaling [cite: 128]
# Note: Path may vary slightly based on Jetpack version (VPI 2 vs 3)
find_package(vpi REQUIRED)

# OpenCV (Mainly for calibration data loading and debug display)
find_package(OpenCV REQUIRED)

# TensorRT (For Inference)
# Often requires manual path setting on Jetson if not in standard path
include_directories(/usr/include/aarch64-linux-gnu)
link_directories(/usr/lib/aarch64-linux-gnu)

# Threads & SocketCAN
find_package(Threads REQUIRED)

# --- Include Directories ---
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/../common
    ${CUDA_INCLUDE_DIRS}
    ${VPI_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

# --- Source Files ---
file(GLOB_RECURSE SOURCES 
    "src/*.cpp" 
    "src/*.cu"
)

# --- Executable ---
add_executable(vespa_vision ${SOURCES})

# --- Linking ---
target_link_libraries(vespa_vision PRIVATE
    ${CUDA_LIBRARIES}
    ${OpenCV_LIBS}
    vpi
    nvinfer          # TensorRT
    nvparsers        # TensorRT Parsers
    pthread
)

# Compiler flags for Jetson Orin (Ampere Architecture)
target_compile_options(vespa_vision PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-gencode arch=compute_87,code=sm_87>)