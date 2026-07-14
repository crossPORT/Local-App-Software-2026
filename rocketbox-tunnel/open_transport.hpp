#pragma once

#include "rocketbox/sdk.h"

#include <memory>

/** Resolve silkscreen Port 1–4 without opening USB (0 = auto if one cable). */
int resolve_tunnel_display_port(rocketbox::TransportMode mode, int display_port);

/** Open USB by cable serial (display_port 0 = auto if one cable) or sim by Port. */
std::unique_ptr<rocketbox::RocketBoxTransport> open_tunnel_transport(
    rocketbox::TransportMode mode, int display_port);
