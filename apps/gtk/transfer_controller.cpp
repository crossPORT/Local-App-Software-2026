#include "transfer_controller.h"

#include "fabric_meta_file.h"

#include <cstdio>
#include <chrono>
#include <libusb-1.0/libusb.h>
#include <thread>
#include <unistd.h>

namespace {

std::string kind_label(TransferKind kind) {
    switch (kind) {
        case TransferKind::Send:
            return "Send";
        case TransferKind::Receive:
            return "Receive";
        case TransferKind::SendMetaThenFile:
            return "Send";
        case TransferKind::ReceiveMetaThenFile:
            return "Receive";
        case TransferKind::Loopback:
            return "Loopback";
    }
    return "Transfer";
}

}  // namespace

void TransferController::pause_before_payload() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.status_message =
            "Metadata sent — waiting for receiver to open payload path…";
    }
    publish_state();
    std::this_thread::sleep_for(std::chrono::seconds(3));
}

TransferController::TransferController(int port_index, UiUpdateCallback on_ui_update)
    : on_ui_update_(std::move(on_ui_update))
    , port_index_(port_index) {
    if (libusb_init(&usb_ctx_) != 0) {
        usb_ctx_ = nullptr;
        state_.error_message = "Failed to initialize libusb";
        publish_state();
    }
}

TransferController::~TransferController() {
    shutting_down_.store(true, std::memory_order_release);
    if (worker_.joinable()) {
        worker_.join();
    }
    if (usb_ctx_) {
        libusb_exit(usb_ctx_);
        usb_ctx_ = nullptr;
    }
}

bool TransferController::should_emit_ui() const {
    return !shutting_down_.load(std::memory_order_acquire);
}

bool TransferController::is_busy() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_.busy;
}

void TransferController::send_file(const std::string& path) {
    start_worker(TransferKind::Send, path);
}

void TransferController::receive_file(const std::string& out_path) {
    start_worker(TransferKind::Receive, out_path);
}

void TransferController::send_transfer(const std::string& source_path,
                                       const FabricSendMeta& meta,
                                       const std::string& source_root) {
    start_worker(TransferKind::SendMetaThenFile, source_path, meta, source_root);
}

void TransferController::receive_to_directory(const std::string& target_dir) {
    start_worker(TransferKind::ReceiveMetaThenFile, target_dir);
}

void TransferController::loopback_test(const std::string& path) {
    start_worker(TransferKind::Loopback, path);
}

int TransferController::fabric_device_count() const {
    if (!usb_ctx_) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(usb_mutex_);
    return count_fabric_devices(usb_ctx_);
}

bool TransferController::fabric_device_bus_addr(int port_index,
                                                uint8_t* bus,
                                                uint8_t* addr) const {
    if (!usb_ctx_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(usb_mutex_);
    return ::fabric_device_bus_addr(usb_ctx_, port_index, bus, addr);
}

std::vector<FabricUsbDevice> TransferController::list_fabric_devices() const {
    if (!usb_ctx_) {
        return {};
    }
    std::lock_guard<std::mutex> lock(usb_mutex_);
    return ::list_fabric_devices(usb_ctx_);
}

void TransferController::start_worker(TransferKind kind,
                                      const std::string& path,
                                      const FabricSendMeta& meta,
                                      const std::string& source_root) {
    if (worker_.joinable()) {
        worker_.join();
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_.busy) {
            return;
        }
        state_.busy = true;
        state_.waiting_for_sender = (kind == TransferKind::Receive
                                     || kind == TransferKind::ReceiveMetaThenFile);
        state_.bytes_done = 0;
        state_.bytes_total = 0;
        state_.elapsed_secs = 0.0;
        state_.live_mbps = 0.0;
        state_.result_mbps = 0.0;
        state_.last_success = false;
        state_.error_message.clear();
        state_.status_message = state_.waiting_for_sender
            ? "Waiting for sender..."
            : (kind == TransferKind::Loopback)
            ? "Loopback: starting receive on port 1..."
            : kind_label(kind) + " in progress...";
    }
    progress_seen_.store(false, std::memory_order_release);
    transfer_detail_.clear();
    publish_state();

    worker_ = std::thread([this, kind, path, meta, source_root]() {
        ProgressCallback progress = [this](uint64_t done, uint64_t total, double elapsed) {
            if (!should_emit_ui()) {
                return;
            }
            const bool first_progress = !progress_seen_.exchange(true, std::memory_order_acq_rel);

            TransferUiState snapshot;
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (first_progress) {
                    state_.waiting_for_sender = false;
                }
                state_.bytes_done = done;
                state_.bytes_total = total;
                state_.elapsed_secs = elapsed;
                state_.live_mbps = (elapsed > 0.0 && done > 0)
                    ? (static_cast<double>(done) / (1024.0 * 1024.0)) / elapsed
                    : 0.0;
                if (total > 0) {
                    const int pct = static_cast<int>((done * 100) / total);
                    state_.status_message = "Transferring... " + std::to_string(pct) + "%";
                } else {
                    state_.status_message = "Transferring...";
                }
                snapshot = state_;
            }
            if (on_ui_update_) {
                on_ui_update_(snapshot);
            }
        };

        const auto run_usb_locked = [&](const auto& fn) {
            std::lock_guard<std::mutex> lock(usb_mutex_);
            fn();
        };

        TransferResult result{};
        switch (kind) {
            case TransferKind::Send:
                run_usb_locked([&] {
                    result = send_file_core(usb_ctx_, path, port_index_, progress);
                });
                break;
            case TransferKind::Receive:
                run_usb_locked([&] {
                    result = receive_file_core(usb_ctx_, path, port_index_, progress);
                });
                break;
            case TransferKind::SendMetaThenFile: {
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    state_.status_message = "Sending metadata (type=file)…";
                }
                publish_state();

                const std::string meta_path = write_meta_temp_file(meta);
                if (meta_path.empty()) {
                    result.ok = false;
                    result.error_message = "Could not create metadata file";
                    break;
                }
                TransferResult meta_result{};
                run_usb_locked([&] {
                    meta_result = send_file_core(usb_ctx_, meta_path, port_index_, nullptr);
                });
                std::remove(meta_path.c_str());
                if (!meta_result.ok) {
                    result = meta_result;
                    break;
                }

                pause_before_payload();

                progress_seen_.store(false, std::memory_order_release);
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    state_.status_message = "Streaming payload…";
                    state_.bytes_done = 0;
                    state_.bytes_total = 0;
                }
                publish_state();
                run_usb_locked([&] {
                    result = send_file_core(usb_ctx_, path, port_index_, progress);
                });
                break;
            }
            case TransferKind::ReceiveMetaThenFile: {
                char meta_template[] = "/tmp/slsfabric-meta-recv-XXXXXX";
                if (mkstemp(meta_template) < 0) {
                    result.ok = false;
                    result.error_message = "Could not create temp path for metadata";
                    break;
                }
                const std::string meta_recv_path(meta_template);
                TransferResult meta_result{};
                run_usb_locked([&] {
                    meta_result =
                        receive_file_core(usb_ctx_, meta_recv_path, port_index_, nullptr);
                });
                if (!meta_result.ok) {
                    std::remove(meta_recv_path.c_str());
                    result = meta_result;
                    break;
                }
                FabricSendMeta received_meta{};
                if (!read_meta_file(meta_recv_path, received_meta)) {
                    std::remove(meta_recv_path.c_str());
                    result.ok = false;
                    result.error_message = "Received metadata file was invalid";
                    break;
                }
                std::remove(meta_recv_path.c_str());

                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    state_.waiting_for_sender = false;
                    state_.status_message = "Metadata OK — receiving "
                                              + received_meta.relative_name;
                }
                publish_state();

                progress_seen_.store(false, std::memory_order_release);
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    state_.bytes_done = 0;
                    state_.bytes_total = 0;
                }

                const std::string out_path =
                    target_path_for_name(path, received_meta.relative_name);
                if (out_path.empty()) {
                    result.ok = false;
                    result.error_message =
                        "Invalid target path for: " + received_meta.relative_name;
                    break;
                }
                {
                    std::string dir_error;
                    if (!ensure_parent_directories(out_path, &dir_error)) {
                        result.ok = false;
                        result.error_message = dir_error;
                        break;
                    }
                }
                run_usb_locked([&] {
                    result = receive_file_core(usb_ctx_, out_path, port_index_, progress);
                });
                if (result.ok) {
                    transfer_detail_ = out_path;
                }
                break;
            }
            case TransferKind::Loopback:
                run_usb_locked([&] {
                    result = loopback_transfer_core(usb_ctx_, path, 0, 1, progress);
                });
                break;
        }

        finish_transfer(result, kind);
    });
}

void TransferController::publish_state() {
    if (!should_emit_ui()) {
        return;
    }
    TransferUiState snapshot;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        snapshot = state_;
    }
    if (on_ui_update_) {
        on_ui_update_(snapshot);
    }
}

void TransferController::finish_transfer(const TransferResult& result, TransferKind kind) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_.busy = false;
        state_.waiting_for_sender = false;
        state_.bytes_done = result.bytes_transferred;
        state_.bytes_total = result.expected_bytes;
        state_.elapsed_secs = result.seconds;
        state_.result_mbps = result.mbps;
        state_.live_mbps = result.mbps;
        state_.last_success = result.ok;

        if (result.ok) {
            if (kind == TransferKind::Loopback) {
                state_.status_message = "Loopback verified — fabric path OK";
            } else if (kind == TransferKind::ReceiveMetaThenFile && !transfer_detail_.empty()) {
                state_.status_message = "Receive complete → " + transfer_detail_;
            } else {
                state_.status_message = kind_label(kind) + " complete";
            }
            state_.error_message.clear();
        } else {
            state_.status_message = kind_label(kind) + " failed";
            state_.error_message = result.error_message;
        }
    }
    publish_state();
}
