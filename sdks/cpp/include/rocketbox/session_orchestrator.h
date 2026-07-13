#pragma once

#include "transfer_orchestrator.h"

#include "rocketbox/sdk.h"

#include <memory>

namespace rocketbox {

/** Shared transport for SessionOrchestrator (display_port = zero-based port + 1). */
inline std::shared_ptr<RocketBoxTransport> make_shared_transport(TransportMode mode,
                                                                int zero_based_port) {
    return std::shared_ptr<RocketBoxTransport>(
        create_rocketbox_transport(mode, zero_based_port + 1).release());
}

}  // namespace rocketbox
