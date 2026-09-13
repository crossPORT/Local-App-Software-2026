#include "usb_plane.hpp"

namespace rocketbox {
namespace detail {

FileTransferResult UsbPlane::send_file_on_port(int port, const std::string& path,
                                               FileProgressFn progress, unsigned timeout_ms,
                                               uint8_t frame_kind) {
    ensure_controller();
    return controller_->send_on_port(port, path, std::move(progress), timeout_ms, frame_kind);
}

FileTransferResult UsbPlane::receive_file_on_port(int port, const std::string& path,
                                                  FileProgressFn progress,
                                                  unsigned header_timeout_ms,
                                                  uint8_t expected_frame_kind) {
    ensure_controller();
    return controller_->receive_on_port(port, path, std::move(progress), header_timeout_ms,
                                        expected_frame_kind);
}

FileTransferResult UsbPlane::loopback_files(const std::string& path, int send_port, int recv_port,
                                            FileProgressFn progress) {
    ensure_controller();
    return controller_->loopback_on_ports(path, send_port, recv_port, std::move(progress));
}

}  // namespace detail
}  // namespace rocketbox
