#pragma once

#include "usb_transfer.h"
#include "fabric_meta_file.h"

#include <cstdint>
#include <vector>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

struct libusb_context;

enum class TransferKind {
    Send,
    Receive,
    SendMetaThenFile,
    ReceiveMetaThenFile,
    Loopback,
};

struct TransferUiState {
    bool        busy = false;
    bool        waiting_for_sender = false;
    bool        last_success = false;
    uint64_t    bytes_done = 0;
    uint64_t    bytes_total = 0;
    double      elapsed_secs = 0.0;
    double      live_mbps = 0.0;
    double      result_mbps = 0.0;
    std::string status_message;
    std::string error_message;
};

class TransferController {
public:
    using UiUpdateCallback = std::function<void(const TransferUiState&)>;

    // All USB/libusb work runs on a dedicated worker thread (see start_worker).
    // UiUpdateCallback may be invoked from that thread — UI code must marshal to
    // the main thread (wxWidgets: wxTheApp->CallAfter).
    TransferController(int port_index, UiUpdateCallback on_ui_update);
    ~TransferController();

    TransferController(const TransferController&) = delete;
    TransferController& operator=(const TransferController&) = delete;

    bool is_busy() const;
    int port_index() const { return port_index_; }

    void send_file(const std::string& path);
    void receive_file(const std::string& out_path);
    void send_transfer(const std::string& source_path,
                       const FabricSendMeta& meta,
                       const std::string& source_root = {});
    void receive_to_directory(const std::string& target_dir);
    void loopback_test(const std::string& path);
    TransferResult send_on_port(int port_index,
                                const std::string& path,
                                ProgressCallback progress_cb = nullptr,
                                unsigned timeout_ms = usb_protocol::kFileTimeoutMs);
    TransferResult receive_on_port(int port_index,
                                   const std::string& path,
                                   ProgressCallback progress_cb = nullptr,
                                   unsigned header_timeout_ms = usb_protocol::kFileTimeoutMs);
    TransferResult loopback_on_ports(const std::string& path,
                                     int send_port_index,
                                     int recv_port_index,
                                     ProgressCallback progress_cb = nullptr);
    void run_payload_send(const std::string& path, ProgressCallback progress_cb = nullptr);
    void run_payload_receive(const std::string& out_path,
                             ProgressCallback progress_cb = nullptr);
    int fabric_device_count() const;
    bool fabric_port_available() const;
    std::string fabric_device_label() const;
    void request_shutdown();
    bool is_shutting_down() const;
    bool fabric_device_bus_addr(int port_index, uint8_t* bus, uint8_t* addr) const;
    std::vector<FabricUsbDevice> list_fabric_devices() const;

private:
    void start_worker(TransferKind kind,
                      const std::string& path,
                      const FabricSendMeta& meta = {},
                      const std::string& source_root = {});
    void publish_state();
    void finish_transfer(const TransferResult& result, TransferKind kind);
    void pause_before_payload();
    bool should_emit_ui() const;

    UiUpdateCallback on_ui_update_;
    int port_index_ = 0;
    libusb_context* usb_ctx_ = nullptr;

    mutable std::mutex state_mutex_;
    mutable std::timed_mutex usb_mutex_;
    TransferUiState state_;

    std::thread worker_;
    std::atomic<bool> shutting_down_{false};
    std::atomic<bool> progress_seen_{false};
    mutable std::atomic<bool> last_port_present_{false};
    mutable std::atomic<int> last_device_count_{0};
    mutable std::string cached_device_label_;
    std::string transfer_detail_;
};
