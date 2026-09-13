#pragma once

#include "rocketbox/sdk.h"
#include "transfer_controller.h"

#include <memory>
#include <string>

namespace rocketbox {
namespace detail {

class UsbPlane final : public RocketBoxTransport {
public:
    explicit UsbPlane(int display_port);
    ~UsbPlane() override;

    bool connected() const override { return connected_; }
    int port_index() const override;
    int resolved_port_index() const override;
    std::string serial() const override;
    std::string describe_device() const override;
    void connect() override;
    void disconnect() override;
    void reset_connection() override;

    int device_count() const override;
    bool port_available() const override;
    void request_shutdown() override;
    bool is_shutting_down() const override;
    bool is_transfer_busy() const override;

    FileTransferResult send_file_on_port(int port, const std::string& path, FileProgressFn progress,
                                         unsigned timeout_ms, uint8_t frame_kind) override;
    FileTransferResult receive_file_on_port(int port, const std::string& path,
                                            FileProgressFn progress, unsigned header_timeout_ms,
                                            uint8_t expected_frame_kind) override;
    FileTransferResult loopback_files(const std::string& path, int send_port, int recv_port,
                                      FileProgressFn progress) override;

private:
    void ensure_controller();

    int display_port_;
    int port_index_;
    bool connected_ = false;
    std::unique_ptr<TransferController> controller_;
};

}  // namespace detail
}  // namespace rocketbox
