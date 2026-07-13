#include "fabric_port.h"

#include <sstream>
#include <vector>

int display_port_from_leg(int leg) {
    return leg + 1;
}

std::string format_fabric_port_label(int leg) {
    std::ostringstream out;
    out << "Port " << display_port_from_leg(leg);
    return out.str();
}

int fabric_leg_from_serial(const std::string& serial) {
    const std::string trimmed = serial;
    if (trimmed.empty()) {
        return -1;
    }
    try {
        const unsigned long value = std::stoul(trimmed, nullptr, 16);
        if (value <= 0) {
            return -1;
        }
        return static_cast<int>((value - 1) % kFabricLegCount);
    } catch (...) {
        return -1;
    }
}

int default_remote_guess_leg(int my_leg) {
    return (my_leg + 1) % kFabricLegCount;
}

/** State-1 idle partner display port (1–4): 1↔2, 3↔4. */
int default_pair_display_port(int display_port) {
    switch (display_port) {
        case 1:
            return 2;
        case 2:
            return 1;
        case 3:
            return 4;
        case 4:
            return 3;
        default:
            return 0;
    }
}

std::vector<int> remote_fabric_legs(int my_leg) {
    std::vector<int> legs;
    legs.reserve(kFabricLegCount - 1);
    for (int leg = 0; leg < kFabricLegCount; ++leg) {
        if (leg != my_leg) {
            legs.push_back(leg);
        }
    }
    return legs;
}
