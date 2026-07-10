#pragma once

#include "protocol.hpp"

#include <libusb-1.0/libusb.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace rocketbox {
namespace detail {

struct UsbEndpoints {
    uint8_t ep1_out = USB_EP1_OUT;
    uint8_t ep2_in = USB_EP2_IN;
    uint8_t ep3_in = USB_EP3_IN;
    uint8_t ep4_out = USB_EP4_OUT;
};

inline UsbEndpoints discover_usb_endpoints(libusb_device_handle* handle) {
    libusb_device* dev = libusb_get_device(handle);
    libusb_config_descriptor* cfg = nullptr;
    if (libusb_get_active_config_descriptor(dev, &cfg) != 0 || !cfg) {
        return {};
    }
    std::vector<uint8_t> outs, inns;
    if (cfg->bNumInterfaces > 0) {
        const auto& iface = cfg->interface[USB_INTERFACE];
        if (iface.num_altsetting > 0) {
            const auto& alt = iface.altsetting[0];
            for (int i = 0; i < alt.bNumEndpoints; ++i) {
                const auto& ep = alt.endpoint[i];
                if ((ep.bmAttributes & 0x03) != LIBUSB_TRANSFER_TYPE_BULK) continue;
                if (ep.bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                    inns.push_back(ep.bEndpointAddress);
                } else {
                    outs.push_back(ep.bEndpointAddress);
                }
            }
        }
    }
    libusb_free_config_descriptor(cfg);

    UsbEndpoints e;
    if (outs.size() >= 2 && inns.size() >= 2) {
        std::sort(outs.begin(), outs.end());
        std::sort(inns.begin(), inns.end());
        e.ep1_out = outs[0];
        e.ep4_out = outs[1];
        e.ep2_in = inns[0];
        e.ep3_in = inns[1];
        for (uint8_t a : outs) {
            if ((a & 0x7F) == 1) e.ep1_out = a;
            if ((a & 0x7F) == 4) e.ep4_out = a;
        }
        for (uint8_t a : inns) {
            if ((a & 0x7F) == 2) e.ep2_in = a;
            if ((a & 0x7F) == 3) e.ep3_in = a;
        }
        return e;
    }
    if (outs.size() == 1 && inns.size() == 1) {
        throw std::runtime_error(
            "Device exposes only 2 bulk endpoints (legacy ROCKETBX). "
            "IntelliConnex 4-endpoint firmware is required.");
    }
    return e;
}

inline void append_bytes(std::vector<uint8_t>& buf, const uint8_t* data, int n) {
    buf.insert(buf.end(), data, data + n);
}

}  // namespace detail
}  // namespace rocketbox
