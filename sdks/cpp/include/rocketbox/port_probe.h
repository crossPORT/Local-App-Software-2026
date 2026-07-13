#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct libusb_context;

namespace rocketbox {

/** One enumerated RocketBox USB cable (libusb order + serial-derived display port). */
struct PresentPort {
    int libusb_index = -1;   // pass create_rocketbox_transport(Usb, libusb_index + 1)
    int leg = -1;            // 0–3 from serial; -1 if unreadable
    int display_port = 0;    // leg + 1; 0 if unknown
    std::string serial;
    uint8_t bus = 0;
    uint8_t addr = 0;
    bool available = false;  // claimable now (false if App/tunnel holds it)
};

/** Probe without owning a transport. ctx=nullptr → ephemeral libusb init/exit. */
std::vector<PresentPort> list_present_ports(libusb_context* ctx = nullptr);

/** Serial → fabric leg 0–3; -1 if invalid (same as session port_index_from_serial). */
int leg_from_serial(const std::string& serial);

/** First claimable libusb index, or -1. */
int auto_pick_libusb_index(libusb_context* ctx = nullptr);

/** Libusb index whose serial maps to silkscreen Port N (1–4), or -1. */
int libusb_index_for_display_port(int display_port, libusb_context* ctx = nullptr);

}  // namespace rocketbox
