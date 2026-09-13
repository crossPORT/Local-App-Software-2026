#pragma once

#include "rocketbox/sdk.h"
#include "transfer_orchestrator.h"

#include <memory>
#include <utility>

namespace rocketbox {

inline std::unique_ptr<TransferOrchestrator> make_usb_orchestrator(
    int zero_based_port,
    IdentityProfile identity,
    TransferOrchestrator::UiCallback on_ui_update) {
    return std::make_unique<TransferOrchestrator>(
        make_usb_transport(zero_based_port), std::move(identity), std::move(on_ui_update));
}

}  // namespace rocketbox
