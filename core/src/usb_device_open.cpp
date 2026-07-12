#include "usb_device_open.h"

#include "usb_protocol.h"
#include "usb_transfer.h"

#include <cstdio>
#include <libusb-1.0/libusb.h>
#include <thread>
#include <vector>

libusb_device_handle* open_device_by_index(libusb_context* ctx,
                                           int index,
                                           std::string* error_out,
                                           int max_open_attempts) {
    const std::vector<FabricUsbDevice> devices = list_fabric_devices(ctx);
    if (index < 0 || index >= static_cast<int>(devices.size())) {
        if (error_out) {
            *error_out = "Could not open USB device at port index "
                + std::to_string(index) + " (found " + std::to_string(devices.size())
                + " fabric device(s))";
        }
        return nullptr;
    }

    const FabricUsbDevice target = devices[static_cast<std::size_t>(index)];

    libusb_device** list = nullptr;
    const ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        if (error_out) {
            *error_out = "Could not enumerate USB devices";
        }
        return nullptr;
    }

    libusb_device* target_dev = nullptr;
    for (ssize_t i = 0; i < count; ++i) {
        if (libusb_get_bus_number(list[i]) == target.bus
            && libusb_get_device_address(list[i]) == target.addr) {
            target_dev = list[i];
            break;
        }
    }

    if (!target_dev) {
        libusb_free_device_list(list, 1);
        if (error_out) {
            *error_out = "Fabric device at port index " + std::to_string(index)
                + " is no longer connected";
        }
        return nullptr;
    }

    libusb_device_handle* handle = nullptr;
    int rc = LIBUSB_ERROR_OTHER;
    for (int attempt = 0; attempt < max_open_attempts; ++attempt) {
        rc = libusb_open(target_dev, &handle);
        if (rc == LIBUSB_SUCCESS) {
            break;
        }
        if (rc == LIBUSB_ERROR_BUSY && attempt + 1 < max_open_attempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }
        handle = nullptr;
        break;
    }

    libusb_free_device_list(list, 1);

    if (!handle) {
        if (error_out) {
            if (rc == LIBUSB_ERROR_ACCESS) {
                *error_out =
                    "USB permission denied. Run once: "
                    "./scripts/setup-usb-access.sh "
                    "(then unplug/replug the cable)";
            } else if (rc == LIBUSB_ERROR_BUSY) {
                *error_out =
                    "USB device busy (another app may be using this cable). "
                    "Close other RocketBox windows and retry.";
            } else {
                *error_out = "USB open failed, status=" + std::to_string(rc);
            }
        }
        return nullptr;
    }

    if (libusb_kernel_driver_active(handle, usb_protocol::kInterface) == 1) {
        fprintf(stderr, "[USB-DIAG] detaching kernel driver on port %d\n", index);
        libusb_detach_kernel_driver(handle, usb_protocol::kInterface);
    }

    int claim_rc = libusb_claim_interface(handle, usb_protocol::kInterface);
    fprintf(stderr, "[USB-DIAG] claim_interface port=%d rc=%d (%s)\n",
            index, claim_rc, libusb_strerror(static_cast<libusb_error>(claim_rc)));
    if (claim_rc != LIBUSB_SUCCESS) {
        libusb_close(handle);
        if (error_out) {
            *error_out = "Could not claim USB interface";
        }
        return nullptr;
    }

    int ch_out = libusb_clear_halt(handle, usb_protocol::kEndpointDataOut);
    int ch_in = libusb_clear_halt(handle, usb_protocol::kEndpointDataIn);
    fprintf(stderr, "[USB-DIAG] clear_halt port=%d EP_OUT=%d (%s) EP_IN=%d (%s)\n",
            index, ch_out, libusb_strerror(static_cast<libusb_error>(ch_out)),
            ch_in, libusb_strerror(static_cast<libusb_error>(ch_in)));

    return handle;
}

void close_device(libusb_device_handle* handle) {
    if (!handle) {
        return;
    }
    libusb_release_interface(handle, usb_protocol::kInterface);
    libusb_close(handle);
}
