#include "usb_plane.hpp"

#include "platform_util.h"
#include "usb_protocol.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace rocketbox {
namespace detail {

FileTransferResult UsbPlane::from_core(const TransferResult& r) { return r; }

UsbPlane::UsbPlane(int display_port)
    : display_port_(display_port), port_index_(display_port - 1) {
    if (display_port_ < 1 || display_port_ > 4) {
        throw std::runtime_error("display port must be 1–4");
    }
}

UsbPlane::~UsbPlane() { disconnect(); }

int UsbPlane::port_index() const {
    // Libusb sort index used to open the handle — not the serial-derived display leg.
    return port_index_;
}

int UsbPlane::resolved_port_index() const {
    return controller_ ? controller_->resolved_port_index() : port_index_;
}

void UsbPlane::connect() {
    if (connected_) {
        return;
    }
    // port_index_ is the libusb sort index (wx ResolvePortIndex), matching
    // working TransferOrchestrator(port_index) → TransferController(port_index).
    controller_ = std::make_unique<TransferController>(port_index_, [](const TransferUiState&) {});
    if (controller_->device_count() <= 0) {
        controller_.reset();
        throw std::runtime_error("no RocketBox USB device");
    }
    // Eager serial→leg while the interface is still free (693da0a fabric_leg).
    (void)controller_->rocketbox_device_serial();
    (void)controller_->resolved_port_index();
    connected_ = true;
}

void UsbPlane::disconnect() {
    if (controller_) {
        try {
            controller_->switch_port(0);
        } catch (...) {
        }
        controller_->request_shutdown();
    }
    controller_.reset();
    connected_ = false;
}

void UsbPlane::reset_connection() {
    disconnect();
    connect();
}

std::string UsbPlane::serial() const {
    return controller_ ? controller_->rocketbox_device_serial() : "";
}

std::string UsbPlane::system_id() const {
    const int display = resolved_port_index() + 1;
    return system_id_for_display_port(display >= 1 && display <= 4 ? display : display_port_);
}

std::string UsbPlane::describe_device() const {
    return controller_ ? controller_->device_label() : "RocketBox USB";
}

int UsbPlane::device_count() const {
    return controller_ ? controller_->device_count() : 0;
}

bool UsbPlane::port_available() const {
    return controller_ && controller_->rocketbox_port_available();
}

void UsbPlane::request_shutdown() {
    if (controller_) {
        controller_->request_shutdown();
    }
}

bool UsbPlane::is_shutting_down() const {
    return !controller_ || controller_->is_shutting_down();
}

bool UsbPlane::is_transfer_busy() const {
    return controller_ && controller_->is_busy();
}

void UsbPlane::sync_systems(std::function<void(const std::vector<SystemInfo>&)> handler) {
    std::vector<SystemInfo> systems;
    for (int p = 1; p <= 4; ++p) {
        if (p == display_port_) {
            continue;
        }
        systems.push_back({system_id_for_display_port(p), "Port " + std::to_string(p), "reachable"});
    }
    if (handler) {
        handler(systems);
    }
}

void UsbPlane::ensure_circuit(const std::string& peer_system_id) {
    const int dest = peer_port_from_system_id(peer_system_id);
    auto r = switch_port_if_needed(dest);
    if (!r.ok) {
        throw std::runtime_error(r.error_message.empty() ? "ensure_circuit failed" : r.error_message);
    }
}

void UsbPlane::clear_circuit() { (void)switch_port(0); }

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

}  // namespace detail
}  // namespace rocketbox
