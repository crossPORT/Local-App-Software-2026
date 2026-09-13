#include "usb_plane.hpp"

namespace rocketbox {
namespace detail {

UsbPlane::UsbPlane(int display_port)
    : display_port_(display_port)
    , port_index_(display_port - 1) {
    ensure_controller();
}

UsbPlane::~UsbPlane() {
    request_shutdown();
}

void UsbPlane::ensure_controller() {
    if (!controller_) {
        controller_ = std::make_unique<TransferController>(port_index_, nullptr);
        connected_ = true;
    }
}

int UsbPlane::port_index() const {
    return controller_ ? controller_->port_index() : port_index_;
}

int UsbPlane::resolved_port_index() const {
    return controller_ ? controller_->fabric_leg() : port_index_;
}

std::string UsbPlane::serial() const {
    return controller_ ? controller_->fabric_device_serial() : std::string{};
}

std::string UsbPlane::describe_device() const {
    return controller_ ? controller_->fabric_device_label() : std::string{};
}

void UsbPlane::connect() { ensure_controller(); }

void UsbPlane::disconnect() {
    if (controller_) {
        controller_->request_shutdown();
    }
    connected_ = false;
}

void UsbPlane::reset_connection() {
    disconnect();
    controller_.reset();
    connected_ = false;
    ensure_controller();
}

int UsbPlane::device_count() const {
    return controller_ ? controller_->fabric_device_count() : 0;
}

bool UsbPlane::port_available() const {
    return controller_ && controller_->fabric_port_available();
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

}  // namespace detail
}  // namespace rocketbox
