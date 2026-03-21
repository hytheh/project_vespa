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

# ==============================================================================
# UTILITIES & CALIBRATION TOOLS
# ==============================================================================

# --- Executable: test_camera_hal ---
add_executable(test_camera_hal src/test_camera_hal.cpp src/CameraInterface.cpp)
target_include_directories(test_camera_hal BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    /usr/src/jetson_multimedia_api/include
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(test_camera_hal pthread i2c nvbufsurface ${OpenCV_LIBS})

# --- Executable: test_hardware_sync ---
add_executable(test_hardware_sync 
    src/test_hardware_sync.cpp 
    src/HardwareSyncController.cpp 
    src/CameraInterface.cpp
)
target_include_directories(test_hardware_sync BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    /usr/src/jetson_multimedia_api/include
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(test_hardware_sync pthread i2c nvbufsurface ${OpenCV_LIBS})

# --- Executable: test_stereo_pipeline ---
add_executable(test_stereo_pipeline 
    src/test_stereo_pipeline.cpp 
    src/ArUcoTracker.cpp 
    src/StereoTriangulator.cpp
)
target_include_directories(test_stereo_pipeline BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_stereo_pipeline ${OpenCV_LIBS})
target_compile_definitions(test_stereo_pipeline PRIVATE DEBUG_MODE)

# --- Executable: calibration_bridge ---
add_executable(calibration_bridge 
    src/calibration_bridge.cpp 
    src/HardwareSyncController.cpp 
    src/CameraInterface.cpp
)
target_include_directories(calibration_bridge BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    /usr/src/jetson_multimedia_api/include
)
target_link_libraries(calibration_bridge ${OpenCV_LIBS} zmq pthread)

# --- Executable: stereo_tracker (Live Viewport Backend) ---
add_executable(stereo_tracker 
    src/stereo_tracker.cpp 
    src/HardwareSyncController.cpp
    src/CameraInterface.cpp
    src/ArUcoTracker.cpp
    src/StereoTriangulator.cpp
    src/PredictiveTurretSight.cpp
)
target_include_directories(stereo_tracker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(stereo_tracker PRIVATE DEBUG_MODE)
target_link_libraries(stereo_tracker ${OpenCV_LIBS} zmq)

# ==============================================================================
# FINAL PRODUCTION NODE: vision_node
# ==============================================================================
add_executable(vision_node 
    src/main.cpp 
    src/HardwareSyncController.cpp
    src/CameraInterface.cpp
    src/ArUcoTracker.cpp
    src/StereoTriangulator.cpp
    src/PredictiveTurretSight.cpp
    src/vision_can.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/can_utils.c
)

target_include_directories(vision_node PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# Pure, highly optimized headless binary (No UI, No ZeroMQ)
target_link_libraries(vision_node ${OpenCV_LIBS})

message(STATUS "VESPA Build Configured for Jetson Orin Nano (JetPack 6.2).")