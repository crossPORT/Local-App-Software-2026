#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct libusb_context;

namespace rocketbox {

struct PresentPort {
    int libusb_index = -1;
    int leg = -1;
    int display_port = 0;
    std::string serial;
    uint8_t bus = 0;
    uint8_t addr = 0;
    bool available = false;
};

std::vector<PresentPort> list_present_ports(libusb_context* ctx = nullptr);
int leg_from_serial(const std::string& serial);
int auto_pick_libusb_index(libusb_context* ctx = nullptr);
int libusb_index_for_display_port(int display_port, libusb_context* ctx = nullptr);

}  // namespace rocketbox
