#include "Tracker.h"
#include <iostream>

namespace VESPA {
    Tracker::Tracker() {}
    Tracker::~Tracker() {}
    void Tracker::init() { std::cout << "[Tracker] Initialized." << std::endl; }
    void Tracker::tick() {}
}
