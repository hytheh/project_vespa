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

# Exclude standalone tests from the main executable sources
list(FILTER SOURCES EXCLUDE REGEX "
.*test_camera_hal.cpp|
.*test_stereo_pipeline.cpp|
.*test_hardware_sync.cpp
")

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

# --- Executable: test_hardware_sync ---
# Bootstraps the dual-camera rig and tests the 60Hz PWM heartbeat
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

# Linked libraries
target_link_libraries(test_hardware_sync 
    pthread 
    i2c 
    nvbufsurface
    ${OpenCV_LIBS}
)

# --- Executable: test_stereo_pipeline ---
# Compiles the Phase 3 synthetic test harness
add_executable(test_stereo_pipeline 
    src/test_stereo_pipeline.cpp 
    src/ArUcoTracker.cpp 
    src/StereoTriangulator.cpp
)

target_include_directories(test_stereo_pipeline BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Link OpenCV for ArUco and matrix operations
target_link_libraries(test_stereo_pipeline 
    ${OpenCV_LIBS}
)

# Enable console debug outputs for our Tracker and Triangulator
target_compile_definitions(test_stereo_pipeline PRIVATE DEBUG_MODE)

# --- Executable: calibration bridge ---
#
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

target_link_libraries(calibration_bridge 
    ${OpenCV_LIBS}
    zmq
    pthread 
)

# --- Executable: stereo_tracker ---
add_executable(stereo_tracker 
    src/stereo_tracker.cpp 
    src/HardwareSyncController.cpp 
    src/CameraInterface.cpp
    src/ArUcoTracker.cpp 
    src/StereoTriangulator.cpp
)

target_include_directories(stereo_tracker BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    /usr/src/jetson_multimedia_api/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(stereo_tracker 
    pthread 
    i2c 
    nvbufsurface
    ${OpenCV_LIBS}
    zmq 
)

# Enable the ZMQ Live Stream and debug outputs
target_compile_definitions(stereo_tracker PRIVATE DEBUG_MODE)

# --- Executable: sparse_tracker ---
add_executable(sparse_tracker src/sparse_tracker.cpp src/HardwareSyncController.cpp src/CameraInterface.cpp)

target_include_directories(sparse_tracker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(sparse_tracker ${OpenCV_LIBS})



# --- Executable: turret_tracker (MVP Predictive Aiming + CAN) ---
add_executable(turret_tracker 
    src/turret_tracker.cpp 
    src/PredictiveTurretSight.cpp 
    src/vision_can.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/can_utils.c
)

# Tell the compiler to look in the local folders AND the shared common folder
target_include_directories(turret_tracker PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(turret_tracker ${OpenCV_LIBS})


message(STATUS "VESPA Build Configured for Jetson Orin Nano (JetPack 6.2).")