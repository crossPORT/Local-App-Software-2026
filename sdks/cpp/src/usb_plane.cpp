#include "usb_plane.hpp"

#include "platform_util.h"

#include <stdexcept>

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
    if (stream_mode_) {
        controller_->set_stream_mode(true);
    }
    if (controller_->device_count() <= 0) {
        controller_.reset();
        throw std::runtime_error("no RocketBox USB device");
    }
    // Cache serial→leg BEFORE stream claim. Windows often cannot reopen the same
    // device for a string descriptor while stream_dev_ holds it — empty serial
    // then falls back to libusb index and looks like the wrong Port.
    (void)controller_->rocketbox_device_serial();
    (void)controller_->resolved_port_index();
    if (stream_mode_) {
        std::string warm_err;
        if (!controller_->warm_stream_device(&warm_err)) {
            controller_.reset();
            throw std::runtime_error(warm_err.empty() ? "stream USB open failed" : warm_err);
        }
    }
    connected_ = true;
    bool want_listen = false;
    {
        std::lock_guard<std::mutex> lock(listen_mu_);
        want_listen = static_cast<bool>(on_msg_);
    }
    if (want_listen) {
        start_listen();
    }
}

void UsbPlane::disconnect() {
    stop_listen();
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

}  // namespace detail
}  // namespace rocketbox
