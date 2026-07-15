#include "usb_plane.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace rocketbox {
namespace detail {

void UsbPlane::ensure_circuit(const std::string& peer_system_id) {
    const int dest = peer_port_from_system_id(peer_system_id);
    if (controller_ && controller_->last_switch_dest() == dest) {
        controller_->mark_switch_preserve();
        return;
    }
    // EP4 connect: one switch packet when dest changes (HW). Settle after listen resumes.
    {
        ListenUsbPause pause(*this);
        auto r = switch_port(dest);
        if (!r.ok) {
            throw std::runtime_error(r.error_message.empty() ? "ensure_circuit failed"
                                                             : r.error_message);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    if (controller_) {
        controller_->mark_switch_preserve();
    }
}

void UsbPlane::clear_circuit() {
    ListenUsbPause pause(*this);
    (void)switch_port(0);
}

int UsbPlane::switch_dest() const {
    return controller_ ? controller_->last_switch_dest() : 0;
}

FileTransferResult UsbPlane::switch_port(int dest_port) {
    if (!controller_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "not connected"};
    }
    return from_core(controller_->switch_port(dest_port));
}

FileTransferResult UsbPlane::switch_port_if_needed(int dest_port) {
    if (!controller_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "not connected"};
    }
    return from_core(controller_->switch_port_if_needed(dest_port));
}

void UsbPlane::mark_switch_preserve() {
    if (controller_) {
        controller_->mark_switch_preserve();
    }
}

bool UsbPlane::switch_preserve() const {
    return controller_ && controller_->switch_preserve();
}

void UsbPlane::wait_for_idle() {
    while (controller_ && controller_->is_busy()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void UsbPlane::set_stream_mode(bool enabled) {
    stream_mode_ = enabled;
    if (controller_) {
        controller_->set_stream_mode(enabled);
    }
}

}  // namespace detail
}  // namespace rocketbox
