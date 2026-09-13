#include "rocketbox/sdk.h"
#include "usb_plane.hpp"

#include <stdexcept>

namespace rocketbox {

std::unique_ptr<RocketBoxTransport> create_rocketbox_transport(int display_port) {
    if (display_port < 1) {
        throw std::runtime_error("display_port must be >= 1");
    }
    return std::make_unique<detail::UsbPlane>(display_port);
}

}  // namespace rocketbox
