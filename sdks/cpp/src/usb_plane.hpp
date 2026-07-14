#pragma once

#include "rocketbox/sdk.h"
#include "rocketbox_frame.hpp"
#include "transfer_controller.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rocketbox {
namespace detail {

/** USB backend via TransferController (sole owner of libusb for this transport). */
class UsbPlane final : public RocketBoxTransport {
public:
    explicit UsbPlane(int display_port);
    ~UsbPlane() override;

    bool connected() const override { return connected_; }
    int port_index() const override;
    int resolved_port_index() const override;
    std::string serial() const override;
    std::string system_id() const override;
    std::string describe_device() const override;
    void connect() override;
    void disconnect() override;
    void reset_connection() override;

    void set_listen_mode(ListenMode) override {}
    void ensure_listening() override;
    void sync_systems(std::function<void(const std::vector<SystemInfo>&)> handler) override;
    void ensure_circuit(const std::string& peer_system_id) override;
    void clear_circuit() override;
    int switch_dest() const override;
    void send_session_message(const std::vector<uint8_t>& message) override;
    void send_announce_presence(const std::vector<uint8_t>& message, AnnouncePresenceMode) override;
    bool try_receive_session_message(unsigned header_timeout_ms, std::vector<uint8_t>* out) override;

    void send_bytes(const std::vector<uint8_t>& payload, const std::string& filename) override;
    bool exchange_bytes(const std::vector<uint8_t>& request, std::vector<uint8_t>* reply,
                        unsigned reply_timeout_ms) override;
    ParsedHeader receive_header() override;
    std::vector<uint8_t> receive_payload(uint64_t file_size) override;
    std::vector<uint8_t> receive_bytes() override;
    void prepare_for_payload_send() override {}
    void wait_for_idle() override;
    void on_data_message(std::function<void(const std::vector<uint8_t>&)> cb) override;

    int device_count() const override;
    bool port_available() const override;
    void request_shutdown() override;
    bool is_shutting_down() const override;
    bool is_transfer_busy() const override;
    FileTransferResult switch_port(int dest_port) override;
    FileTransferResult switch_port_if_needed(int dest_port) override;
    void mark_switch_preserve() override;
    bool switch_preserve() const override;
    FileTransferResult send_file_on_port(int port, const std::string& path, FileProgressFn progress,
                                         unsigned timeout_ms, uint8_t frame_kind) override;
    FileTransferResult receive_file_on_port(int port, const std::string& path,
                                            FileProgressFn progress, unsigned header_timeout_ms,
                                            uint8_t expected_frame_kind) override;
    FileTransferResult loopback_files(const std::string& path, int send_port, int recv_port,
                                      FileProgressFn progress) override;
    void set_stream_mode(bool enabled) override;

private:
    void send_raw_file(const std::vector<uint8_t>& bytes, uint8_t frame_kind,
                       const std::string& filename);
    std::vector<uint8_t> recv_raw_file(uint8_t expected_kind);
    static FileTransferResult from_core(const TransferResult& r);
    void start_listen();
    void stop_listen();
    void listen_loop();

    int display_port_;
    int port_index_;
    bool connected_ = false;
    bool stream_mode_ = false;
    std::unique_ptr<TransferController> controller_;
    std::vector<uint8_t> pending_payload_;
    std::mutex listen_mu_;
    std::function<void(const std::vector<uint8_t>&)> on_msg_;
    std::atomic<bool> listen_stop_{true};
    std::thread listen_thread_;
};

}  // namespace detail
}  // namespace rocketbox
