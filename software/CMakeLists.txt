cmake_minimum_required(VERSION 3.14)
project(VESPA_Vision_Node LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Dependencies ---
find_package(CUDA REQUIRED)
find_package(OpenCV REQUIRED) # Used mainly for matrix utils, not processing
find_package(VPI 3.0 REQUIRED)

# --- Include Directories ---
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/../common
    ${CUDA_INCLUDE_DIRS}
    ${VPI_INCLUDE_DIRS}
    /usr/include/libdrm # Often needed for NvBufSurface
)

# --- Library Paths ---
link_directories(
    /usr/lib/aarch64-linux-gnu/tegra
    /usr/local/cuda/lib64
)

# --- Source Files ---
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE COMMON_SOURCES "../common/*.cpp")

# --- Executable ---
add_executable(vespa_vision ${SOURCES} ${COMMON_SOURCES})

# --- Linking ---
target_link_libraries(vespa_vision
    ${OpenCV_LIBS}
    ${CUDA_LIBRARIES}
    vpi
    nvinfer          # TensorRT
    nvparsers        # TensorRT Parsers
    nvbuf_utils      # Zero-Copy buffers
    pthread
)

message(STATUS "VESPA Build Configured for Jetson Orin.")