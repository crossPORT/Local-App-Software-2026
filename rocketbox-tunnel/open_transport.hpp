#pragma once

#include "rocketbox/sdk.h"

#include <memory>

/** Open USB by cable serial (display_port 0 = auto if one cable) or sim by Port. */
std::unique_ptr<rocketbox::RocketBoxTransport> open_tunnel_transport(
    rocketbox::TransportMode mode, int display_port);
