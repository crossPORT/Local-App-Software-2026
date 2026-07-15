#include "transfer_controller.h"

#include "event_log.h"
#include "link_policy.h"
#include "port_util.h"
#include "rocketbox_sim.h"
#include "usb_device_open.h"
#include "usb_protocol.h"
#include "usb_transfer_handle.h"

#include "meta_file.h"
#include "platform_util.h"

#include <cstdio>
#include <chrono>
#include <fstream>
#include <thread>
#include <libusb-1.0/libusb.h>
#include <sstream>

namespace {

constexpr unsigned kSessionSendTimeoutMs = 2500;
constexpr auto kUsbLockWait = std::chrono::milliseconds(2000);

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

bool TransferController::lock_usb_in(UsbTimedLock& lock, std::chrono::milliseconds wait) const {
    lock = UsbTimedLock(usb_in_mutex_, std::defer_lock);
    return lock.try_lock_for(wait);
}

bool TransferController::lock_usb_out(UsbTimedLock& lock, std::chrono::milliseconds wait) const {
    lock = UsbTimedLock(usb_out_mutex_, std::defer_lock);
    return lock.try_lock_for(wait);
}

bool TransferController::lock_usb_both(UsbTimedLock& in_lock, UsbTimedLock& out_lock,
                                       std::chrono::milliseconds wait) const {
    in_lock = UsbTimedLock(usb_in_mutex_, std::defer_lock);
    out_lock = UsbTimedLock(usb_out_mutex_, std::defer_lock);
    if (!in_lock.try_lock_for(wait)) {
        return false;
    }
    if (!out_lock.try_lock_for(wait)) {
        in_lock.unlock();
        return false;
    }
    return true;
}

bool TransferController::warm_stream_if_needed(std::string* err) {
    if (!stream_mode_ || stream_dev_) {
        return true;
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, kUsbLockWait)) {
        if (err) {
            *err = "USB port busy";
        }
        return false;
    }
    return ensure_stream_device(err);
}

void TransferController::request_shutdown() {
    shutting_down_.store(true, std::memory_order_release);
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (lock_usb_both(in_lock, out_lock, std::chrono::milliseconds(500))) {
        release_stream_device();
    }
}

bool TransferController::is_shutting_down() const {
    return shutting_down_.load(std::memory_order_acquire);
}

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
    release_stream_device();
    if (usb_ctx_) {
        libusb_exit(usb_ctx_);
        usb_ctx_ = nullptr;
    }
}

bool TransferController::ensure_stream_device(std::string* err) {
    if (stream_dev_) return true;
    stream_dev_ = open_device_by_index(usb_ctx_, port_index_, err, 5, false);
    return stream_dev_ != nullptr;
}

void TransferController::release_stream_device() {
    if (!stream_dev_) return;
    close_device(stream_dev_);
    stream_dev_ = nullptr;
}

void TransferController::set_stream_mode(bool enabled) {
    if (stream_mode_ == enabled) return;
    if (!enabled) {
        UsbTimedLock in_lock;
        UsbTimedLock out_lock;
        if (lock_usb_both(in_lock, out_lock, kUsbLockWait)) {
            release_stream_device();
        }
    }
    stream_mode_ = enabled;
}

bool TransferController::warm_stream_device(std::string* err) {
    if (!stream_mode_ || !usb_ctx_) return false;
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, kUsbLockWait)) {
        if (err) *err = "USB port busy";
        return false;
    }
    return ensure_stream_device(err);
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
                                       const SendMeta& meta,
                                       const std::string& source_root) {
    start_worker(TransferKind::SendMetaThenFile, source_path, meta, source_root);
}

void TransferController::receive_to_directory(const std::string& target_dir) {
    start_worker(TransferKind::ReceiveMetaThenFile, target_dir);
}

void TransferController::loopback_test(const std::string& path) {
    start_worker(TransferKind::Loopback, path);
}

TransferResult TransferController::send_on_port(int port_index,
                                                const std::string& path,
                                                ProgressCallback progress_cb,
                                                unsigned timeout_ms,
                                                uint8_t frame_kind) {
    if (!usb_ctx_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "libusb not initialized"};
    }
    if (shutting_down_.load(std::memory_order_acquire)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "Shutting down"};
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, kUsbLockWait)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "USB port busy"};
    }
    TransferResult result = rocketbox_sim_enabled()
        ? rocketbox_sim_send_file(path, port_index, std::move(progress_cb), timeout_ms)
        : send_file_core(usb_ctx_, path, port_index, std::move(progress_cb), timeout_ms,
                         frame_kind, {}, !stream_mode_);
    event_log(resolved_port_index(),
              result.ok ? "usb_send_ok" : "usb_send_fail",
              path + " bytes=" + std::to_string(result.bytes_transferred)
                  + (result.error_message.empty() ? "" : " err=" + result.error_message));
    return result;
}

TransferResult TransferController::receive_on_port(int port_index,
                                                   const std::string& path,
                                                   ProgressCallback progress_cb,
                                                   unsigned header_timeout_ms,
                                                   uint8_t expected_frame_kind) {
    if (!usb_ctx_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "libusb not initialized"};
    }
    if (shutting_down_.load(std::memory_order_acquire)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "Shutting down"};
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, kUsbLockWait)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "USB port busy"};
    }
    TransferResult result = rocketbox_sim_enabled()
        ? rocketbox_sim_receive_file(path, port_index, std::move(progress_cb), header_timeout_ms)
        : receive_file_core(usb_ctx_, path, port_index, std::move(progress_cb),
                            header_timeout_ms, expected_frame_kind, !stream_mode_);
    if (!result.ok && header_timeout_ms <= usb_protocol::kSessionHeaderTimeoutMs + 1) {
        // Session listener polls frequently; only log non-timeout failures.
        if (result.error_message != "Header read failed") {
            event_log(resolved_port_index(),
                      "usb_recv_fail",
                      path + " err=" + result.error_message);
        }
    } else if (result.ok) {
        event_log(resolved_port_index(),
                  "usb_recv_ok",
                  path + " bytes=" + std::to_string(result.bytes_transferred));
    }
    return result;
}

TransferResult TransferController::send_buffer(int port_index, const uint8_t* data, size_t len,
                                               unsigned timeout_ms, uint8_t frame_kind) {
    if (!usb_ctx_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "libusb not initialized"};
    }
    if (shutting_down_.load(std::memory_order_acquire)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "Shutting down"};
    }
    if (stream_mode_ && port_index == port_index_ && !rocketbox_sim_enabled()) {
        std::string warm_err;
        if (!warm_stream_if_needed(&warm_err)) {
            return TransferResult{false, 0, 0, 0.0, 0.0,
                                  warm_err.empty() ? "stream open failed" : warm_err};
        }
    }
    UsbTimedLock out_lock;
    if (!lock_usb_out(out_lock, kUsbLockWait)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "USB port busy"};
    }
    if (rocketbox_sim_enabled()) {
        const std::string path = platform::create_empty_temp_file("rocketbox-sim-send-");
        if (path.empty()) {
            return TransferResult{false, 0, 0, 0.0, 0.0, "temp file failed"};
        }
        {
            std::ofstream out(path, std::ios::binary);
            if (len > 0) {
                out.write(reinterpret_cast<const char*>(data),
                          static_cast<std::streamsize>(len));
            }
        }
        auto result = rocketbox_sim_send_file(path, port_index, nullptr, timeout_ms);
        std::remove(path.c_str());
        event_log(resolved_port_index(), result.ok ? "usb_send_ok" : "usb_send_fail",
                  "buffer bytes=" + std::to_string(result.bytes_transferred) +
                      (result.error_message.empty() ? "" : " err=" + result.error_message));
        return result;
    }
    TransferResult result;
    if (stream_mode_ && port_index == port_index_) {
        if (!stream_dev_) {
            return TransferResult{false, 0, 0, 0.0, 0.0, "stream not open"};
        }
        result = send_buffer_on_handle(stream_dev_, data, len, timeout_ms, frame_kind, nullptr);
    } else {
        result = send_buffer_core(usb_ctx_, data, len, port_index, timeout_ms, frame_kind, nullptr,
                                  !stream_mode_);
    }
    event_log(resolved_port_index(), result.ok ? "usb_send_ok" : "usb_send_fail",
              "buffer bytes=" + std::to_string(result.bytes_transferred) +
                  (result.error_message.empty() ? "" : " err=" + result.error_message));
    return result;
}

TransferResult TransferController::receive_buffer(int port_index, std::vector<uint8_t>* out,
                                                  unsigned header_timeout_ms,
                                                  uint8_t expected_frame_kind) {
    if (!usb_ctx_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "libusb not initialized"};
    }
    if (shutting_down_.load(std::memory_order_acquire)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "Shutting down"};
    }
    if (stream_mode_ && port_index == port_index_ && !rocketbox_sim_enabled()) {
        std::string warm_err;
        if (!warm_stream_if_needed(&warm_err)) {
            return TransferResult{false, 0, 0, 0.0, 0.0,
                                  warm_err.empty() ? "stream open failed" : warm_err};
        }
    }
    UsbTimedLock in_lock;
    if (!lock_usb_in(in_lock, kUsbLockWait)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "USB port busy"};
    }
    if (rocketbox_sim_enabled()) {
        const std::string path = platform::create_empty_temp_file("rocketbox-sim-recv-");
        if (path.empty() || !out) {
            return TransferResult{false, 0, 0, 0.0, 0.0, "temp file failed"};
        }
        auto result =
            rocketbox_sim_receive_file(path, port_index, nullptr, header_timeout_ms);
        if (result.ok) {
            std::ifstream in(path, std::ios::binary);
            *out = std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        } else if (out) {
            out->clear();
        }
        std::remove(path.c_str());
        if (result.ok) {
            event_log(resolved_port_index(), "usb_recv_ok",
                      "buffer bytes=" + std::to_string(result.bytes_transferred));
        } else if (result.error_message != "Header read failed") {
            event_log(resolved_port_index(), "usb_recv_fail",
                      "buffer err=" + result.error_message);
        }
        return result;
    }
    TransferResult result;
    if (stream_mode_ && port_index == port_index_) {
        if (!stream_dev_) {
            return TransferResult{false, 0, 0, 0.0, 0.0, "stream not open"};
        }
        result = receive_buffer_on_handle(stream_dev_, out, header_timeout_ms, expected_frame_kind);
    } else {
        result = receive_buffer_core(usb_ctx_, out, port_index, header_timeout_ms,
                                     expected_frame_kind, !stream_mode_);
    }
    if (result.ok) {
        event_log(resolved_port_index(), "usb_recv_ok",
                  "buffer bytes=" + std::to_string(result.bytes_transferred));
    } else if (result.error_message != "Header read failed") {
        event_log(resolved_port_index(), "usb_recv_fail",
                  "buffer err=" + result.error_message);
    }
    return result;
}

TransferResult TransferController::exchange_buffer(int port_index, const uint8_t* data, size_t len,
                                                   std::vector<uint8_t>* reply,
                                                   unsigned reply_timeout_ms, uint8_t frame_kind) {
    if (!usb_ctx_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "libusb not initialized"};
    }
    if (!reply) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "null reply"};
    }
    if (shutting_down_.load(std::memory_order_acquire)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "Shutting down"};
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, kUsbLockWait)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "USB port busy"};
    }
    reply->clear();
    if (rocketbox_sim_enabled()) {
        const std::string spath = platform::create_empty_temp_file("rocketbox-sim-ex-s-");
        const std::string rpath = platform::create_empty_temp_file("rocketbox-sim-ex-r-");
        if (spath.empty() || rpath.empty()) {
            return TransferResult{false, 0, 0, 0.0, 0.0, "temp file failed"};
        }
        {
            std::ofstream out(spath, std::ios::binary);
            if (len > 0) {
                out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
            }
        }
        auto sent = rocketbox_sim_send_file(spath, port_index, nullptr, usb_protocol::kFileTimeoutMs);
        std::remove(spath.c_str());
        if (!sent.ok) {
            std::remove(rpath.c_str());
            return sent;
        }
        auto got = rocketbox_sim_receive_file(rpath, port_index, nullptr, reply_timeout_ms);
        if (got.ok) {
            std::ifstream in(rpath, std::ios::binary);
            *reply = std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
        }
        std::remove(rpath.c_str());
        return got;
    }
    std::string err;
    if (stream_mode_ && port_index == port_index_) {
        if (!ensure_stream_device(&err)) {
            return TransferResult{false, 0, 0, 0.0, 0.0, err.empty() ? "stream open failed" : err};
        }
        auto sent = send_buffer_on_handle(stream_dev_, data, len, usb_protocol::kFileTimeoutMs,
                                          frame_kind, nullptr);
        event_log(resolved_port_index(), sent.ok ? "usb_send_ok" : "usb_send_fail",
                  "exchange bytes=" + std::to_string(sent.bytes_transferred) +
                      (sent.error_message.empty() ? "" : " err=" + sent.error_message));
        if (!sent.ok) return sent;
        auto got = receive_buffer_on_handle(stream_dev_, reply, reply_timeout_ms,
                                            usb_protocol::kFrameKindPayload);
        if (got.ok) {
            event_log(resolved_port_index(), "usb_recv_ok",
                      "exchange bytes=" + std::to_string(got.bytes_transferred));
        } else {
            event_log(resolved_port_index(), "usb_recv_fail",
                      "exchange err=" + got.error_message);
        }
        return got;
    }
    auto sent = send_buffer_core(usb_ctx_, data, len, port_index, usb_protocol::kFileTimeoutMs,
                                 frame_kind, nullptr, true);
    if (!sent.ok) return sent;
    return receive_buffer_core(usb_ctx_, reply, port_index, reply_timeout_ms,
                               usb_protocol::kFrameKindPayload, true);
}

TransferResult TransferController::loopback_on_ports(const std::string& path,
                                                     int send_port_index,
                                                     int recv_port_index,
                                                     ProgressCallback progress_cb) {
    if (!usb_ctx_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "libusb not initialized"};
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, kUsbLockWait)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "USB port busy"};
    }
    return rocketbox_sim_enabled()
        ? rocketbox_sim_loopback(path, send_port_index, recv_port_index, std::move(progress_cb))
        : loopback_transfer_core(
              usb_ctx_, path, send_port_index, recv_port_index, std::move(progress_cb));
}

TransferResult TransferController::switch_port(int dest_port) {
    if (!usb_ctx_) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "libusb not initialized"};
    }
    if (shutting_down_.load(std::memory_order_acquire)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "Shutting down"};
    }
    if (rocketbox_sim_enabled()) {
        event_log(resolved_port_index(), "switch_port", "sim skip dest=" + std::to_string(dest_port));
        last_switch_dest_ = dest_port;
        if (dest_port == 0) {
            switch_preserve_ = false;
        }
        return TransferResult{true, 16, 16, 0.0, 0.0, {}};
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, kUsbLockWait)) {
        return TransferResult{false, 0, 0, 0.0, 0.0, "USB port busy"};
    }
    TransferResult result;
    if (stream_mode_) {
        std::string err;
        if (!ensure_stream_device(&err)) {
            return TransferResult{false, 0, 0, 0.0, 0.0, err.empty() ? "stream open failed" : err};
        }
        result = switch_port_on_handle(stream_dev_, dest_port);
    } else {
        result = switch_port_core(usb_ctx_, port_index_, dest_port, true);
    }
    if (result.ok) {
        last_switch_dest_ = dest_port;
        if (dest_port == 0) {
            switch_preserve_ = false;
        }
    }
    event_log(resolved_port_index(),
              result.ok ? "switch_ok" : "switch_fail",
              "dest=" + std::to_string(dest_port)
                  + (result.error_message.empty() ? "" : " err=" + result.error_message));
    return result;
}

TransferResult TransferController::switch_port_if_needed(int dest_port) {
    if (last_switch_dest_ == dest_port) {
        return TransferResult{true, 0, 0, 0.0, 0.0, {}};
    }
    TransferResult result = switch_port(dest_port);
    if (result.ok && dest_port >= 1 && dest_port <= 4) {
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return result;
}

void TransferController::mark_switch_preserve() {
    switch_preserve_ = last_switch_dest_ >= 1 && last_switch_dest_ <= 4;
}

void TransferController::clear_switch_dest_cache() {
    last_switch_dest_ = -1;
    switch_preserve_ = false;
}

void TransferController::run_payload_send(const std::string& path,
                                        ProgressCallback progress_cb) {
    start_worker(TransferKind::Send, path);
    (void)progress_cb;
}

void TransferController::run_payload_receive(const std::string& out_path,
                                             ProgressCallback progress_cb) {
    start_worker(TransferKind::Receive, out_path);
    (void)progress_cb;
}

int TransferController::device_count() const {
    if (!usb_ctx_) {
        return 0;
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, std::chrono::milliseconds(200))) {
        // USB is busy (active transfer or the peer process probing). Reporting
        // zero here would look like an unplug, so keep the last known count.
        return last_device_count_.load(std::memory_order_relaxed);
    }
    const int count = rocketbox_sim_enabled() ? rocketbox_sim_count_devices()
                                           : count_rocketbox_devices(usb_ctx_);
    last_device_count_.store(count, std::memory_order_relaxed);
    return count;
}

bool TransferController::rocketbox_port_available() const {
    if (!usb_ctx_) {
        return false;
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, std::chrono::milliseconds(200))) {
        // USB busy — assume the fabric is still present rather than treating a
        // transient lock collision as a disconnect.
        return link_policy::presence_when_probe_busy(
            last_port_present_.load(std::memory_order_relaxed));
    }
    // Presence by enumeration only. Do NOT open/claim the interface here: with
    // two booth processes probing every tick, a claim-based check makes each
    // side intermittently see the fabric as "lost" when the other side (or our
    // own transfer) holds the interface, which manifests as constant flapping.
    const bool present = rocketbox_sim_enabled()
        ? rocketbox_sim_port_available(port_index_)
        : count_rocketbox_devices(usb_ctx_) > port_index_;
    last_port_present_.store(present, std::memory_order_relaxed);
    return present;
}

std::string TransferController::device_label() const {
    (void)resolved_port_index();
    return cached_device_label_;
}

std::string TransferController::rocketbox_device_serial() const {
    if (!cached_device_serial_.empty() || port_index_ < 0) {
        return cached_device_serial_;
    }
    if (!usb_ctx_) {
        return {};
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    // Serial string needs a quiet bus; short waits raced the session listener.
    if (!lock_usb_both(in_lock, out_lock, std::chrono::milliseconds(2000))) {
        return {};
    }
    const std::string serial = rocketbox_sim_enabled()
        ? std::string()
        : ::rocketbox_device_serial(usb_ctx_, port_index_);
    if (!serial.empty()) {
        cached_device_serial_ = serial;
    }
    return serial.empty() ? cached_device_serial_ : serial;
}

int TransferController::resolved_port_index() const {
    // Working 693da0a fabric_leg(): libusb port_index_ opens the device;
    // serial maps to the display leg (0002 → leg 1 / Port 2). Never cache a
    // fallback from a failed serial read — listener claim races return empty
    // and would permanently announce as Port 1 and echo-reject the real Port 1 peer.
    if (cached_port_index_ >= 0) {
        return cached_port_index_;
    }
    if (port_index_ < 0) {
        return 0;
    }
    const std::string serial = rocketbox_device_serial();
    const int from_serial = port_index_from_serial(serial);
    if (from_serial >= 0) {
        cached_port_index_ = from_serial;
        cached_device_label_ = format_port_label(cached_port_index_);
        return cached_port_index_;
    }
    return port_index_;
}

bool TransferController::rocketbox_device_bus_addr(int port_index,
                                                uint8_t* bus,
                                                uint8_t* addr) const {
    if (!usb_ctx_) {
        return false;
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, std::chrono::milliseconds(200))) {
        return false;
    }
    return rocketbox_sim_enabled()
        ? rocketbox_sim_device_bus_addr(port_index, bus, addr)
        : ::rocketbox_device_bus_addr(usb_ctx_, port_index, bus, addr);
}

std::vector<RocketBoxUsbDevice> TransferController::list_rocketbox_devices() const {
    if (!usb_ctx_) {
        return {};
    }
    UsbTimedLock in_lock;
    UsbTimedLock out_lock;
    if (!lock_usb_both(in_lock, out_lock, std::chrono::milliseconds(200))) {
        return {};
    }
    return rocketbox_sim_enabled() ? rocketbox_sim_list_devices()
                                : ::list_rocketbox_devices(usb_ctx_);
}

void TransferController::start_worker(TransferKind kind,
                                      const std::string& path,
                                      const SendMeta& meta,
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

        TransferResult result{};
        const auto run_usb_locked = [&](const auto& fn) {
            UsbTimedLock in_lock;
            UsbTimedLock out_lock;
            if (!lock_usb_both(in_lock, out_lock, kUsbLockWait)) {
                result = TransferResult{false, 0, 0, 0.0, 0.0, "USB port busy"};
                return;
            }
            fn();
        };

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
                const std::string meta_recv_path =
                    platform::create_empty_temp_file("rocketbox-meta-recv-");
                if (meta_recv_path.empty()) {
                    result.ok = false;
                    result.error_message = "Could not create temp path for metadata";
                    break;
                }
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
                SendMeta received_meta{};
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
