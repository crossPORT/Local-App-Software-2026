#include "rocketbox/port_probe.h"

#include "port_util.h"
#include "usb_transfer.h"

#include <libusb-1.0/libusb.h>

namespace rocketbox {
namespace {

struct CtxGuard {
    libusb_context* ctx = nullptr;
    bool owned = false;
    explicit CtxGuard(libusb_context* in) {
        if (in) {
            ctx = in;
            return;
        }
        if (libusb_init(&ctx) == 0) owned = true;
        else ctx = nullptr;
    }
    ~CtxGuard() {
        if (owned && ctx) libusb_exit(ctx);
    }
};

}  // namespace

int leg_from_serial(const std::string& serial) { return port_index_from_serial(serial); }

std::vector<PresentPort> list_present_ports(libusb_context* ctx_in) {
    CtxGuard g(ctx_in);
    std::vector<PresentPort> out;
    if (!g.ctx) return out;
    const auto devices = list_rocketbox_devices(g.ctx);
    out.reserve(devices.size());
    for (size_t i = 0; i < devices.size(); ++i) {
        PresentPort p;
        p.libusb_index = static_cast<int>(i);
        p.bus = devices[i].bus;
        p.addr = devices[i].addr;
        p.serial = rocketbox_device_serial(g.ctx, p.libusb_index);
        p.leg = leg_from_serial(p.serial);
        p.display_port = p.leg >= 0 ? display_port_from_leg(p.leg) : 0;
        p.available = rocketbox_port_available(g.ctx, p.libusb_index);
        out.push_back(std::move(p));
    }
    return out;
}

int auto_pick_libusb_index(libusb_context* ctx_in) {
    CtxGuard g(ctx_in);
    if (!g.ctx) return -1;
    return resolve_rocketbox_port_index(g.ctx);
}

int libusb_index_for_display_port(int display_port, libusb_context* ctx_in) {
    if (display_port < 1 || display_port > 4) return -1;
    const auto ports = list_present_ports(ctx_in);
    int fallback = -1;
    for (const auto& p : ports) {
        if (p.display_port != display_port) continue;
        if (p.available) return p.libusb_index;
        if (fallback < 0) fallback = p.libusb_index;
    }
    return fallback;
}

}  // namespace rocketbox
