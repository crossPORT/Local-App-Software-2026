#include "usb_protocol.h"

#include <cstdio>
#include <libusb-1.0/libusb.h>

int main() {
    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        std::fprintf(stderr, "libusb_init failed\n");
        return 1;
    }

    libusb_device** list = nullptr;
    const ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        std::fprintf(stderr, "libusb_get_device_list failed\n");
        libusb_exit(ctx);
        return 1;
    }

    int matches = 0;
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        if (desc.idVendor != usb_protocol::kVendorId
            || desc.idProduct != usb_protocol::kProductId) {
            continue;
        }

        std::printf("match[%d] bus=%u addr=%u\n",
                    matches,
                    libusb_get_bus_number(list[i]),
                    libusb_get_device_address(list[i]));

        libusb_config_descriptor* config = nullptr;
        if (libusb_get_active_config_descriptor(list[i], &config) != 0) {
            ++matches;
            continue;
        }

        for (int iface = 0; iface < config->bNumInterfaces; ++iface) {
            const libusb_interface& interface = config->interface[iface];
            for (int alt = 0; alt < interface.num_altsetting; ++alt) {
                const libusb_interface_descriptor& setting = interface.altsetting[alt];
                std::printf("  interface %d alt %d endpoints=%u\n",
                            setting.bInterfaceNumber,
                            setting.bAlternateSetting,
                            setting.bNumEndpoints);
                for (int ep = 0; ep < setting.bNumEndpoints; ++ep) {
                    const libusb_endpoint_descriptor& endpoint = setting.endpoint[ep];
                    std::printf("    ep addr=0x%02x type=%u max_packet=%u\n",
                                endpoint.bEndpointAddress,
                                endpoint.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK,
                                endpoint.wMaxPacketSize);
                }
            }
        }

        libusb_free_config_descriptor(config);
        ++matches;
    }

    if (matches == 0) {
        std::printf("No device %04x:%04x found. Plug in the FPGA USB-C cable.\n",
                    usb_protocol::kVendorId,
                    usb_protocol::kProductId);
    } else {
        std::printf("Total matching devices: %d (loopback needs 2 on one PC)\n", matches);
    }

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return matches == 0 ? 2 : 0;
}
