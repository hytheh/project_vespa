#include "CameraInterface.h"
#include <iostream>

namespace VESPA {
    CameraInterface::CameraInterface() {}
    CameraInterface::~CameraInterface() {}
    void CameraInterface::init() { std::cout << "[CameraInterface] Initialized." << std::endl; }
    void CameraInterface::tick() {}
}
