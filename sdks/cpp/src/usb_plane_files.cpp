#include "usb_plane.hpp"

#include "platform_util.h"
#include "usb_protocol.h"

#include <cstdio>
#include <fstream>
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
    const std::string path = platform::create_empty_temp_file("rocketbox-sdk-send-");
    if (path.empty()) {
        throw std::runtime_error("temp file failed");
    }
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    auto r = controller_->send_on_port(port_index(), path, nullptr, usb_protocol::kFileTimeoutMs,
                                       frame_kind);
    std::remove(path.c_str());
    if (!r.ok) {
        throw std::runtime_error(r.error_message.empty() ? "send failed" : r.error_message);
    }
    (void)filename;
}

std::vector<uint8_t> UsbPlane::recv_raw_file(uint8_t expected_kind) {
    if (!controller_) {
        throw std::runtime_error("not connected");
    }
    const std::string path = platform::create_empty_temp_file("rocketbox-sdk-recv-");
    auto r = controller_->receive_on_port(port_index(), path, nullptr, usb_protocol::kFileTimeoutMs,
                                          expected_kind);
    if (!r.ok) {
        std::remove(path.c_str());
        throw std::runtime_error(r.error_message.empty() ? "receive failed" : r.error_message);
    }
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    std::remove(path.c_str());
    return data;
}

void UsbPlane::send_bytes(const std::vector<uint8_t>& payload, const std::string& filename) {
    send_raw_file(payload, usb_protocol::kFrameKindPayload, filename);
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
    const std::string path = platform::create_empty_temp_file("rocketbox-sdk-sess-");
    auto r = controller_->receive_on_port(port_index(), path, nullptr, header_timeout_ms, 0);
    if (!r.ok) {
        std::remove(path.c_str());
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    *out = std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
    std::remove(path.c_str());
    return true;
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
