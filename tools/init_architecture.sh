#!/bin/bash

BASE_DIR="software"
echo "Initializing Project V.E.S.P.A. Software Architecture (Rev 2)..."

# --- Directory Creation ---
mkdir -p $BASE_DIR/common
mkdir -p $BASE_DIR/vision_node/include
mkdir -p $BASE_DIR/vision_node/src
mkdir -p $BASE_DIR/vision_node/models
mkdir -p $BASE_DIR/builds/vision_node
mkdir -p .vscode

# --- 1. Common Protocol Definition (RENAMED) ---
PROTOCOL_FILE="$BASE_DIR/common/CAN.h"
cat <<EOF > "$PROTOCOL_FILE"
/**
 * @file CAN.h
 * @brief Common CAN Bus Protocol Definitions for all nodes.
 */

#ifndef CAN_H
#define CAN_H

#include <stdint.h>

// CAN IDs
#define CAN_ID_VISION_TARGET    0x100
#define CAN_ID_MOTION_STATUS    0x200
#define CAN_ID_SYSTEM_STATE     0x300
#define CAN_ID_LASER_CMD        0x400

typedef struct {
    int16_t azimuth;   // 0.01 degree per bit
    int16_t elevation; // 0.01 degree per bit
    uint16_t distance; // mm
    uint8_t confidence;
    uint8_t flags;
} TargetData_t;

#endif // CAN_H
EOF

# --- 2. Vision Node Headers & Source Wrappers ---

# Helper function
create_class_files() {
    local name=$1
    local desc=$2
    local header="$BASE_DIR/vision_node/include/$name.h"
    local source="$BASE_DIR/vision_node/src/$name.cpp"

    # Header
    cat <<EOF > "$header"
/** @file $name.h | @brief $desc */
#ifndef VESPA_$(echo $name | tr 'a-z' 'A-Z')_H
#define VESPA_$(echo $name | tr 'a-z' 'A-Z')_H

namespace VESPA {
    class $name {
    public:
        $name();
        ~$name();
        void init();
        void tick(); // Main periodic execution
    };
}
#endif
EOF

    # Source
    cat <<EOF > "$source"
#include "$name.h"
#include <iostream>

namespace VESPA {
    $name::$name() {}
    $name::~$name() {}
    void $name::init() { std::cout << "[$name] Initialized." << std::endl; }
    void $name::tick() {}
}
EOF
}

create_class_files "CameraInterface" "Handles V4L2 capture and NvBufSurface management."
create_class_files "InferenceEngine" "Wraps TensorRT engine execution."
create_class_files "Tracker" "Kalman Filter implementation."
create_class_files "Orchestrator" "Main logic controller integrating Capture, Inference and Control."

# --- 3. Minimal Main ---
cat <<EOF > "$BASE_DIR/vision_node/src/main.cpp"
/**
 * @file main.cpp
 * @brief Minimal Bootstrapper for Vision Node
 */
#include "Orchestrator.h"
#include <iostream>
#include <signal.h>

bool volatile g_running = true;

void signal_handler(int signum) {
    g_running = false;
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);
    
    VESPA::Orchestrator app;
    
    try {
        app.init();
        
        while(g_running) {
            app.tick();
            // TODO: Implement proper sleep/wait for interrupt here
        }
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL FAILURE: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
EOF

echo "Structure generation complete. VSCode configs must be created manually or via separate script if not present."
chmod +x init_vespa.sh