#pragma once

#include "rocketbox/transfer_ops.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rocketbox {

/** USB hardware transport (session + ROCKETBX file ops). No TCP/sim plane. */
class RocketBoxTransport {
public:
    virtual ~RocketBoxTransport() = default;

    virtual bool connected() const = 0;
    virtual int port_index() const = 0;
    virtual int resolved_port_index() const { return port_index(); }
    virtual int display_port() const { return resolved_port_index() + 1; }
    virtual std::string serial() const = 0;
    virtual std::string describe_device() const = 0;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual void reset_connection() = 0;

    virtual int device_count() const = 0;
    virtual bool port_available() const = 0;
    virtual void request_shutdown() = 0;
    virtual bool is_shutting_down() const = 0;
    virtual bool is_transfer_busy() const { return false; }

    virtual FileTransferResult send_file_on_port(int port,
                                                 const std::string& path,
                                                 FileProgressFn progress,
                                                 unsigned timeout_ms,
                                                 uint8_t frame_kind);
    virtual FileTransferResult receive_file_on_port(int port,
                                                    const std::string& path,
                                                    FileProgressFn progress,
                                                    unsigned header_timeout_ms,
                                                    uint8_t expected_frame_kind);
    virtual FileTransferResult loopback_files(const std::string& path,
                                              int send_port,
                                              int recv_port,
                                              FileProgressFn progress);
};

/** display_port is silkscreen 1–4; libusb index is display_port - 1. */
std::unique_ptr<RocketBoxTransport> create_rocketbox_transport(int display_port = 1);

inline std::shared_ptr<RocketBoxTransport> make_usb_transport(int zero_based_port) {
    return std::shared_ptr<RocketBoxTransport>(
        create_rocketbox_transport(zero_based_port + 1).release());
}

}  // namespace rocketbox
