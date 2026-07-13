#include "rocketbox_device_picker.h"

#include <libusb-1.0/libusb.h>

int pick_usb_port_index(wxWindow* parent, const std::vector<RocketBoxUsbDevice>& devices) {
    if (devices.empty()) {
        return -1;
    }
    if (devices.size() == 1) {
        return 0;
    }

    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        ctx = nullptr;
    }

    wxArrayString choices;
    for (size_t index = 0; index < devices.size(); ++index) {
        const RocketBoxUsbDevice& device = devices[index];
        const bool available =
            ctx != nullptr && rocketbox_port_available(ctx, static_cast<int>(index));
        const wxString suffix = available ? wxString{} : wxString(" (in use)");
        wxString label;
        label << "USB cable " << static_cast<int>(index + 1) << " — bus "
              << static_cast<unsigned>(device.bus) << " · addr "
              << static_cast<unsigned>(device.addr) << suffix;
        choices.push_back(label);
    }

    if (ctx != nullptr) {
        libusb_exit(ctx);
    }

    const int pick = wxGetSingleChoiceIndex(
        "Choose the USB cable for this app.",
        "Connect USB",
        choices,
        parent);
    return pick;
}
