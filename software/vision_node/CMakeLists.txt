cmake_minimum_required(VERSION 3.14)
project(VESPA_Vision_Node LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
find_package(CUDA REQUIRED)
find_package(OpenCV REQUIRED) 
find_package(VPI 3.0 REQUIRED)

# Check for i2c-dev
find_path(I2C_INCLUDE_DIR i2c/smbus.h)
if(NOT I2C_INCLUDE_DIR)
    message(FATAL_ERROR "i2c/smbus.h not found. Run: sudo apt install libi2c-dev")
endif()

# DeepStream Paths
set(DS_PATH "/opt/nvidia/deepstream/deepstream-7.1/sources/includes")

# --- Link Directories ---
# Explicitly add the Tegra library path for JetPack 6
link_directories("/usr/lib/aarch64-linux-gnu/tegra")

# --- Source Files ---
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE COMMON_SOURCES "../common/*.cpp")

# Exclude the standalone test from the main executable sources
list(FILTER SOURCES EXCLUDE REGEX ".*test_camera_hal.cpp")

# --- Executable: vespa_vision ---
add_executable(vespa_vision ${SOURCES} ${COMMON_SOURCES})

target_include_directories(vespa_vision BEFORE PRIVATE 
    ${DS_PATH}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../common
    ${CUDA_INCLUDE_DIRS}
    ${VPI_INCLUDE_DIRS}
    /usr/include/libdrm
    /usr/src/jetson_multimedia_api/include
)

# Linked libraries (Removed nvbufsurfacemapping)
target_link_libraries(vespa_vision
    ${OpenCV_LIBS}
    ${CUDA_LIBRARIES}
    vpi
    nvinfer
    nvbufsurface    # Main buffer management lib
    i2c
    pthread
)

# --- Executable: test_camera_hal ---
add_executable(test_camera_hal src/test_camera_hal.cpp src/CameraInterface.cpp)

target_include_directories(test_camera_hal BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    /usr/src/jetson_multimedia_api/include
    ${OpenCV_INCLUDE_DIRS}
)

# Linked libraries (Removed nvbufsurfacemapping)
target_link_libraries(test_camera_hal 
    pthread 
    i2c 
    nvbufsurface
    ${OpenCV_LIBS}
)

message(STATUS "VESPA Build Configured for Jetson Orin Nano (JetPack 6.2).")