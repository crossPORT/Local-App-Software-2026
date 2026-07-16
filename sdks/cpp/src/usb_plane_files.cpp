#include "usb_plane.hpp"

#include "usb_protocol.h"

#include <stdexcept>

namespace rocketbox {
namespace detail {
namespace {

ProgressCallback adapt(FileProgressFn progress) { return progress; }

}  // namespace

void UsbPlane::send_raw_file(const std::vector<uint8_t>& bytes, uint8_t frame_kind,
                            const std::string& filename) {
    if (!controller_) {
        throw std::runtime_error("not connected");
    }
    // Pause listen around OUT only: WinUSB times out concurrent bulk IN+OUT
    // ("Header send failed"). EP4 stays aimed for the connection (HW contract).
    const unsigned timeout =
        stream_mode_ ? usb_protocol::kDatagramTimeoutMs : usb_protocol::kFileTimeoutMs;
    auto try_send = [&]() {
        ListenUsbPause pause(*this);
        return controller_->send_buffer(port_index(), bytes.data(), bytes.size(), timeout,
                                        frame_kind);
    };
    auto r = try_send();
    if (!r.ok && stream_mode_) {
        (void)recover_data_path();
        r = try_send();
    }
    if (!r.ok) {
        throw std::runtime_error(r.error_message.empty() ? "send failed" : r.error_message);
    }
    (void)filename;
}

std::vector<uint8_t> UsbPlane::recv_raw_file(uint8_t expected_kind) {
    if (!controller_) {
        throw std::runtime_error("not connected");
    }
    std::vector<uint8_t> data;
    auto r = controller_->receive_buffer(port_index(), &data, usb_protocol::kFileTimeoutMs,
                                         expected_kind);
    if (!r.ok) {
        throw std::runtime_error(r.error_message.empty() ? "receive failed" : r.error_message);
    }
    return data;
}

void UsbPlane::send_bytes(const std::vector<uint8_t>& payload, const std::string& filename) {
    send_raw_file(payload, usb_protocol::kFrameKindPayload, filename);
}

bool UsbPlane::exchange_bytes(const std::vector<uint8_t>& request, std::vector<uint8_t>* reply,
                              unsigned reply_timeout_ms) {
    if (!controller_ || !reply) {
        return false;
    }
    ListenUsbPause pause(*this);
    auto r = controller_->exchange_buffer(port_index(), request.data(), request.size(), reply,
                                          reply_timeout_ms, usb_protocol::kFrameKindPayload);
    return r.ok;
}

void UsbPlane::send_session_message(const std::vector<uint8_t>& message) {
    send_raw_file(message, usb_protocol::kFrameKindSession, {});
}

void UsbPlane::send_announce_presence(const std::vector<uint8_t>& message, AnnouncePresenceMode) {
    send_session_message(message);
}

bool UsbPlane::try_receive_session_message(unsigned header_timeout_ms, std::vector<uint8_t>* out) {
    if (!controller_ || !out) {
        return false;
    }
    auto r = controller_->receive_buffer(port_index(), out, header_timeout_ms, 0);
    return r.ok;
}

ParsedHeader UsbPlane::receive_header() {
    auto body = recv_raw_file(usb_protocol::kFrameKindPayload);
    pending_payload_ = body;
    ParsedHeader h;
    h.file_size = body.size();
    h.frame_kind = usb_protocol::kFrameKindPayload;
    return h;
}

std::vector<uint8_t> UsbPlane::receive_payload(uint64_t) {
    auto out = std::move(pending_payload_);
    pending_payload_.clear();
    if (!out.empty()) {
        return out;
    }
    return recv_raw_file(usb_protocol::kFrameKindPayload);
}

std::vector<uint8_t> UsbPlane::receive_bytes() {
    return recv_raw_file(usb_protocol::kFrameKindPayload);
}

FileTransferResult UsbPlane::send_file_on_port(int port, const std::string& path,
                                               FileProgressFn progress, unsigned timeout_ms,
                                               uint8_t frame_kind) {
    if (!controller_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "not connected"};
    }
    return from_core(controller_->send_on_port(port, path, adapt(progress), timeout_ms, frame_kind));
}

FileTransferResult UsbPlane::receive_file_on_port(int port, const std::string& path,
                                                  FileProgressFn progress,
                                                  unsigned header_timeout_ms,
                                                  uint8_t expected_frame_kind) {
    if (!controller_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "not connected"};
    }
    return from_core(controller_->receive_on_port(port, path, adapt(progress), header_timeout_ms,
                                                   expected_frame_kind));
}

FileTransferResult UsbPlane::loopback_files(const std::string& path, int send_port, int recv_port,
                                            FileProgressFn progress) {
    if (!controller_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "not connected"};
    }
    return from_core(controller_->loopback_on_ports(path, send_port, recv_port, adapt(progress)));
}

}  // namespace detail
}  // namespace rocketbox
