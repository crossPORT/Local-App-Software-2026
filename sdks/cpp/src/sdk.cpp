#include "rocketbox/sdk.h"
#include "sim_plane.hpp"
#include "usb_plane.hpp"

#include <stdexcept>

namespace rocketbox {

std::unique_ptr<RocketBoxTransport> create_rocketbox_transport(TransportMode mode,
                                                               int display_port) {
    if (mode == TransportMode::Sim) {
        return std::make_unique<detail::SimPlane>(display_port);
    }
    if (mode == TransportMode::Usb) {
        return std::make_unique<detail::UsbPlane>(display_port);
    }
    throw std::runtime_error("unknown transport mode");
}

}  // namespace rocketbox
