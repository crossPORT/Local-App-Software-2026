#include "usb_transfer.h"
#include "usb_protocol.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <libusb-1.0/libusb.h>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// Per-chunk payload transfer timeout (data chunks, not the session/header reads).
// Runtime-overridable so deployments can lower it via config; a stalled stream
// (peer powered off mid-transfer while still enumerated) then fails fast.
std::atomic<unsigned> g_payload_timeout_ms{usb_protocol::kFileTimeoutMs};

// Per-process in-flight buffer budget in MB. 0 = auto-detect from the kernel
// usbfs limit. Overridable via config for systems that raised usbfs_memory_mb.
std::atomic<unsigned> g_inflight_budget_mb{0};

// Kernel cap on total usbfs transfer memory (global, shared by every process
// talking to usbfs). Submitting past it returns LIBUSB_ERROR_NO_MEM (-11).
unsigned read_usbfs_limit_mb() {
    std::ifstream f("/sys/module/usbcore/parameters/usbfs_memory_mb");
    unsigned mb = 0;
    if (f >> mb && mb > 0) {
        return mb;
    }
    return 16;  // historical kernel default when the knob is unreadable
}

// How many chunks we may keep in flight without exhausting the usbfs pool.
// Auto mode reserves ~40% of the limit for this process: half is left for the
// peer process (the other transfer direction runs concurrently during a booth
// payload) and the rest is headroom. An explicit budget overrides the estimate.
unsigned effective_queue_depth() {
    unsigned budget_mb = g_inflight_budget_mb.load();
    if (budget_mb == 0) {
        const unsigned limit_mb = read_usbfs_limit_mb();
        budget_mb = (limit_mb * 2) / 5;  // 40% of the shared pool
    }
    const uint64_t budget_bytes = static_cast<uint64_t>(budget_mb) * 1024ull * 1024ull;
    uint64_t depth = budget_bytes / usb_protocol::kChunkSize;
    if (depth < 1) {
        depth = 1;
    }
    if (depth > usb_protocol::kQueueDepth) {
        depth = usb_protocol::kQueueDepth;
    }
    return static_cast<unsigned>(depth);
}

#pragma pack(push, 1)
struct FileHeader {
    uint8_t  magic[8];
    uint64_t file_size;
    uint8_t  frame_kind;
    uint8_t  filename[15];
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == usb_protocol::kHeaderSize,
              "file header must be 32 bytes");

struct AsyncCtx {
    int      done = 0;
    int      status = 0;
    int      actual = 0;
    uint8_t* buf = nullptr;
    int      expected = 0;
};

struct QueuedXfr {
    libusb_transfer*     xfr = nullptr;
    std::vector<uint8_t> buf;
    int                  valid_bytes = 0;
    bool                 in_flight = false;
    bool                 completed = false;
    int                  status = 0;
    int                  actual = 0;

    QueuedXfr() : buf(usb_protocol::kChunkSize, 0x00) {}
};

enum class BufState : int { FREE = 0, IN_FLIGHT = 1, READY = 2 };

struct RingSlot {
    libusb_transfer*     xfr = nullptr;
    std::vector<uint8_t> buf;
    std::atomic<BufState> state{BufState::FREE};
    int                  actual = 0;
    int                  status = 0;
    int                  real_bytes = 0;

    RingSlot() : buf(usb_protocol::kChunkSize, 0x00) {}

    RingSlot(const RingSlot&) = delete;
    RingSlot& operator=(const RingSlot&) = delete;
};

void LIBUSB_CALL async_cb(libusb_transfer* transfer) {
    auto* ctx = static_cast<AsyncCtx*>(transfer->user_data);
    ctx->status = transfer->status;
    ctx->actual = transfer->actual_length;
    ctx->done = 1;
}

void LIBUSB_CALL queued_cb(libusb_transfer* transfer) {
    auto* queued = static_cast<QueuedXfr*>(transfer->user_data);
    queued->status = transfer->status;
    queued->actual = transfer->actual_length;
    queued->completed = true;
    queued->in_flight = false;
}

void LIBUSB_CALL ring_cb(libusb_transfer* transfer) {
    auto* slot = static_cast<RingSlot*>(transfer->user_data);
    slot->actual = transfer->actual_length;
    slot->status = transfer->status;
    std::atomic_thread_fence(std::memory_order_release);
    slot->state.store(BufState::READY, std::memory_order_release);
}

double elapsed_seconds(const Clock::time_point& start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

double mib_per_second(uint64_t bytes, double seconds) {
    if (bytes == 0) {
        return 0.0;
    }
    const double effective_seconds = std::max(seconds, 0.001);
    return (static_cast<double>(bytes) / (1024.0 * 1024.0)) / effective_seconds;
}

TransferResult make_result(bool ok,
                           uint64_t transferred,
                           uint64_t expected,
                           double seconds,
                           const std::string& error_message = "") {
    return TransferResult{
        ok,
        transferred,
        expected,
        seconds,
        mib_per_second(transferred, seconds),
        error_message,
    };
}

libusb_device_handle* open_device_by_index(libusb_context* ctx,
                                           int index,
                                           std::string* error_out = nullptr,
                                           int max_open_attempts = 5) {
    const std::vector<FabricUsbDevice> devices = list_fabric_devices(ctx);
    if (index < 0 || index >= static_cast<int>(devices.size())) {
        if (error_out) {
            *error_out = "Could not open USB device at port index "
                + std::to_string(index) + " (found " + std::to_string(devices.size())
                + " fabric device(s))";
        }
        return nullptr;
    }

    const FabricUsbDevice target = devices[static_cast<std::size_t>(index)];

    libusb_device** list = nullptr;
    const ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        if (error_out) {
            *error_out = "Could not enumerate USB devices";
        }
        return nullptr;
    }

    libusb_device* target_dev = nullptr;
    for (ssize_t i = 0; i < count; ++i) {
        if (libusb_get_bus_number(list[i]) == target.bus
            && libusb_get_device_address(list[i]) == target.addr) {
            target_dev = list[i];
            break;
        }
    }

    if (!target_dev) {
        libusb_free_device_list(list, 1);
        if (error_out) {
            *error_out = "Fabric device at port index " + std::to_string(index)
                + " is no longer connected";
        }
        return nullptr;
    }

    libusb_device_handle* handle = nullptr;
    int rc = LIBUSB_ERROR_OTHER;
    for (int attempt = 0; attempt < max_open_attempts; ++attempt) {
        rc = libusb_open(target_dev, &handle);
        if (rc == LIBUSB_SUCCESS) {
            break;
        }
        if (rc == LIBUSB_ERROR_BUSY && attempt + 1 < max_open_attempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }
        handle = nullptr;
        break;
    }

    libusb_free_device_list(list, 1);

    if (!handle) {
        if (error_out) {
            if (rc == LIBUSB_ERROR_ACCESS) {
                *error_out =
                    "USB permission denied. Run once: "
                    "./scripts/setup-usb-access.sh "
                    "(then unplug/replug the cable)";
            } else if (rc == LIBUSB_ERROR_BUSY) {
                *error_out =
                    "USB device busy (another app may be using this cable). "
                    "Close other RocketBox windows and retry.";
            } else {
                *error_out = "USB open failed, status=" + std::to_string(rc);
            }
        }
        return nullptr;
    }

    if (libusb_kernel_driver_active(handle, usb_protocol::kInterface) == 1) {
        fprintf(stderr, "[USB-DIAG] detaching kernel driver on port %d\n", index);
        libusb_detach_kernel_driver(handle, usb_protocol::kInterface);
    }

    int claim_rc = libusb_claim_interface(handle, usb_protocol::kInterface);
    fprintf(stderr, "[USB-DIAG] claim_interface port=%d rc=%d (%s)\n",
            index, claim_rc, libusb_strerror(static_cast<libusb_error>(claim_rc)));
    if (claim_rc != LIBUSB_SUCCESS) {
        libusb_close(handle);
        if (error_out) {
            *error_out = "Could not claim USB interface";
        }
        return nullptr;
    }

    int ch_out = libusb_clear_halt(handle, usb_protocol::kEndpointDataOut);
    int ch_in  = libusb_clear_halt(handle, usb_protocol::kEndpointDataIn);
    fprintf(stderr, "[USB-DIAG] clear_halt port=%d EP_OUT=%d (%s) EP_IN=%d (%s)\n",
            index, ch_out, libusb_strerror(static_cast<libusb_error>(ch_out)),
            ch_in,  libusb_strerror(static_cast<libusb_error>(ch_in)));

    return handle;
}

void close_device(libusb_device_handle* handle) {
    if (!handle) {
        return;
    }
    libusb_release_interface(handle, usb_protocol::kInterface);
    libusb_close(handle);
}

bool bulk_write_all(libusb_device_handle* handle,
                    const uint8_t* data,
                    std::size_t len,
                    int timeout_ms,
                    std::string* error_out = nullptr) {
    std::size_t offset = 0;
    while (offset < len) {
        int transferred = 0;
        const std::size_t to_write =
            std::min(len - offset, usb_protocol::kChunkSize);
        fprintf(stderr, "[USB-DIAG] bulk_write EP=0x%02x len=%zu timeout=%dms\n",
                usb_protocol::kEndpointDataOut, to_write, timeout_ms);
        const int rc = libusb_bulk_transfer(
            handle,
            usb_protocol::kEndpointDataOut,
            const_cast<uint8_t*>(data + offset),
            static_cast<int>(to_write),
            &transferred,
            timeout_ms);
        fprintf(stderr, "[USB-DIAG] bulk_write result: rc=%d (%s) transferred=%d\n",
                rc, libusb_strerror(static_cast<libusb_error>(rc)), transferred);
        if (rc != LIBUSB_SUCCESS) {
            if (error_out) {
                *error_out = std::string(libusb_strerror(static_cast<libusb_error>(rc)));
            }
            return false;
        }
        offset += static_cast<std::size_t>(transferred);
    }
    return true;
}

bool bulk_read_all(libusb_device_handle* handle,
                   uint8_t* data,
                   std::size_t len,
                   int timeout_ms) {
    std::size_t offset = 0;
    while (offset < len) {
        int transferred = 0;
        const std::size_t to_read =
            std::min(len - offset, usb_protocol::kChunkSize);
        const int rc = libusb_bulk_transfer(
            handle,
            usb_protocol::kEndpointDataIn,
            data + offset,
            static_cast<int>(to_read),
            &transferred,
            timeout_ms);
        if (rc != LIBUSB_SUCCESS) {
            fprintf(stderr, "[USB-DIAG] bulk_read EP=0x%02x len=%zu timeout=%dms rc=%d (%s)\n",
                    usb_protocol::kEndpointDataIn, to_read, timeout_ms,
                    rc, libusb_strerror(static_cast<libusb_error>(rc)));
            return false;
        }
        offset += static_cast<std::size_t>(transferred);
    }
    return true;
}

bool discard_incoming_bytes(libusb_device_handle* handle,
                            uint64_t byte_count,
                            int timeout_ms) {
    constexpr uint64_t kMaxStraySessionBytes = 256 * 1024;
    if (byte_count > kMaxStraySessionBytes) {
        return false;
    }
    std::vector<uint8_t> scratch(
        static_cast<std::size_t>(std::min(byte_count, static_cast<uint64_t>(usb_protocol::kChunkSize))),
        0);
    uint64_t remaining = byte_count;
    while (remaining > 0) {
        const std::size_t chunk =
            static_cast<std::size_t>(std::min(remaining, static_cast<uint64_t>(scratch.size())));
        if (!bulk_read_all(handle, scratch.data(), chunk, timeout_ms)) {
            return false;
        }
        remaining -= chunk;
    }
    return true;
}

int remaining_timeout_ms(const Clock::time_point& deadline) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
    return std::max(1, static_cast<int>(remaining.count()));
}

bool usb_transfer_chunk(libusb_context* ctx,
                        libusb_device_handle* tx_dev,
                        libusb_device_handle* rx_dev,
                        uint8_t* tx_data,
                        uint8_t* rx_data,
                        int len,
                        int* rx_actual) {
    libusb_transfer* xfr_rx = libusb_alloc_transfer(0);
    libusb_transfer* xfr_tx = libusb_alloc_transfer(0);
    AsyncCtx ctx_rx{};
    AsyncCtx ctx_tx{};
    ctx_rx.buf = rx_data;
    ctx_rx.expected = len;
    ctx_tx.buf = tx_data;
    ctx_tx.expected = len;

    libusb_fill_bulk_transfer(
        xfr_rx,
        rx_dev,
        usb_protocol::kEndpointDataIn,
        rx_data,
        len,
        async_cb,
        &ctx_rx,
        static_cast<int>(usb_protocol::kFileTimeoutMs));
    libusb_fill_bulk_transfer(
        xfr_tx,
        tx_dev,
        usb_protocol::kEndpointDataOut,
        tx_data,
        len,
        async_cb,
        &ctx_tx,
        static_cast<int>(usb_protocol::kFileTimeoutMs));

    libusb_submit_transfer(xfr_rx);
    libusb_submit_transfer(xfr_tx);

    while (!ctx_tx.done || !ctx_rx.done) {
        timeval tv{0, 50000};
        libusb_handle_events_timeout(ctx, &tv);
    }

    libusb_free_transfer(xfr_tx);
    libusb_free_transfer(xfr_rx);

    *rx_actual = ctx_rx.actual;

    return ctx_tx.status == LIBUSB_TRANSFER_COMPLETED
        && ctx_rx.status == LIBUSB_TRANSFER_COMPLETED;
}

void fill_file_header(FileHeader* header,
                      uint64_t file_size,
                      uint8_t frame_kind = usb_protocol::kFrameKindPayload,
                      const char* filename = nullptr) {
    std::memcpy(header->magic, usb_protocol::kHeaderMagic, 8);
    header->file_size = file_size;
    header->frame_kind = frame_kind;
    std::memset(header->filename, 0, sizeof(header->filename));
    if (filename) {
        const std::size_t len = std::strlen(filename);
        const std::size_t copy = std::min(len, static_cast<std::size_t>(usb_protocol::kFrameFilenameMax));
        std::memcpy(header->filename, filename, copy);
    }
}

bool files_equal(const std::string& a, const std::string& b) {
    std::ifstream left(a, std::ios::binary);
    std::ifstream right(b, std::ios::binary);
    if (!left || !right) {
        return false;
    }

    left.seekg(0, std::ios::end);
    right.seekg(0, std::ios::end);
    if (left.tellg() != right.tellg()) {
        return false;
    }
    left.seekg(0, std::ios::beg);
    right.seekg(0, std::ios::beg);

    constexpr std::size_t kBuf = 64 * 1024;
    std::vector<char> la(kBuf);
    std::vector<char> ra(kBuf);

    while (left && right) {
        left.read(la.data(), static_cast<std::streamsize>(kBuf));
        right.read(ra.data(), static_cast<std::streamsize>(kBuf));
        const std::streamsize lg = left.gcount();
        const std::streamsize rg = right.gcount();
        if (lg != rg) {
            return false;
        }
        if (lg > 0
            && std::memcmp(la.data(), ra.data(), static_cast<std::size_t>(lg)) != 0) {
            return false;
        }
        if (left.eof() != right.eof()) {
            return false;
        }
    }

    return left.eof() && right.eof();
}

TransferResult file_cross_connect_core(libusb_context* ctx,
                                       const std::string& in_path,
                                       const std::string& out_path,
                                       int send_port_index,
                                       int recv_port_index,
                                       ProgressCallback progress_cb) {
    const auto started = Clock::now();

    std::ifstream in_file(in_path, std::ios::binary | std::ios::ate);
    if (!in_file) {
        return make_result(false, 0, 0, 0.0, "Could not open file: " + in_path);
    }

    const uint64_t file_size = static_cast<uint64_t>(in_file.tellg());
    in_file.seekg(0, std::ios::beg);

    std::ofstream out_file(out_path, std::ios::binary | std::ios::trunc);
    if (!out_file) {
        return make_result(false, 0, file_size, 0.0,
                           "Could not create output file: " + out_path);
    }

    libusb_device_handle* send_handle =
        open_device_by_index(ctx, send_port_index);
    libusb_device_handle* recv_handle =
        open_device_by_index(ctx, recv_port_index);
    if (!send_handle || !recv_handle) {
        close_device(send_handle);
        close_device(recv_handle);
        in_file.close();
        out_file.close();
        return make_result(false, 0, file_size, elapsed_seconds(started),
                           "Could not open both USB devices for loopback");
    }

    FileHeader tx_hdr{};
    fill_file_header(&tx_hdr, file_size);
    std::string write_err;
    if (!bulk_write_all(send_handle,
                        reinterpret_cast<uint8_t*>(&tx_hdr),
                        sizeof(tx_hdr),
                        static_cast<int>(usb_protocol::kFileTimeoutMs),
                        &write_err)) {
        close_device(send_handle);
        close_device(recv_handle);
        in_file.close();
        out_file.close();
        return make_result(false, 0, file_size, elapsed_seconds(started),
                           "Header send failed: " + write_err);
    }

    FileHeader rx_hdr{};
    if (!bulk_read_all(recv_handle,
                       reinterpret_cast<uint8_t*>(&rx_hdr),
                       sizeof(rx_hdr),
                       static_cast<int>(usb_protocol::kFileTimeoutMs))) {
        close_device(send_handle);
        close_device(recv_handle);
        in_file.close();
        out_file.close();
        return make_result(false, 0, file_size, elapsed_seconds(started),
                           "Header read failed");
    }
    if (std::memcmp(rx_hdr.magic, usb_protocol::kHeaderMagic, 8) != 0) {
        close_device(send_handle);
        close_device(recv_handle);
        in_file.close();
        out_file.close();
        return make_result(false, 0, file_size, elapsed_seconds(started),
                           "Bad magic bytes in header");
    }

    struct CrossSlot {
        std::vector<uint8_t> tx_buf;
        std::vector<uint8_t> rx_buf;
        int                  tx_bytes = 0;
        int                  rx_bytes = 0;
        bool                 filled = false;
        bool                 usb_done = false;
        bool                 eof = false;
        std::mutex           mtx;
        std::condition_variable cv;

        CrossSlot()
            : tx_buf(usb_protocol::kChunkSize)
            , rx_buf(usb_protocol::kChunkSize) {}
    };

    CrossSlot slots[2];
    std::atomic<bool> transfer_error{false};
    uint64_t total_bytes_done = 0;

    std::thread reader_thread([&]() {
        uint64_t bytes_read = 0;
        int slot_idx = 0;

        while (bytes_read < file_size && !transfer_error.load()) {
            CrossSlot& slot = slots[slot_idx];
            {
                std::unique_lock<std::mutex> lock(slot.mtx);
                slot.cv.wait(lock, [&] { return !slot.filled; });
            }

            const uint64_t remaining = file_size - bytes_read;
            const int to_read = static_cast<int>(
                std::min(remaining, static_cast<uint64_t>(usb_protocol::kChunkSize)));

            in_file.read(reinterpret_cast<char*>(slot.tx_buf.data()), to_read);
            const int actually_read = static_cast<int>(in_file.gcount());
            if (actually_read <= 0) {
                transfer_error.store(true);
                break;
            }

            bytes_read += static_cast<uint64_t>(actually_read);

            {
                std::lock_guard<std::mutex> lock(slot.mtx);
                slot.tx_bytes = actually_read;
                slot.filled = true;
                slot.usb_done = false;
                slot.eof = (bytes_read >= file_size);
            }
            slot.cv.notify_all();
            slot_idx = 1 - slot_idx;
        }
    });

    std::thread writer_thread([&]() {
        int slot_idx = 0;
        uint64_t bytes_written = 0;

        while (bytes_written < file_size && !transfer_error.load()) {
            CrossSlot& slot = slots[slot_idx];
            {
                std::unique_lock<std::mutex> lock(slot.mtx);
                slot.cv.wait(lock, [&] {
                    return slot.usb_done || transfer_error.load();
                });
                if (transfer_error.load()) {
                    break;
                }
            }

            out_file.write(reinterpret_cast<const char*>(slot.rx_buf.data()),
                           slot.rx_bytes);
            if (!out_file.good()) {
                transfer_error.store(true);
                break;
            }

            bytes_written += static_cast<uint64_t>(slot.rx_bytes);

            {
                std::lock_guard<std::mutex> lock(slot.mtx);
                slot.filled = false;
                slot.usb_done = false;
            }
            slot.cv.notify_all();
            slot_idx = 1 - slot_idx;
        }
    });

    int slot_idx = 0;
    bool all_done = false;

    while (!all_done && !transfer_error.load()) {
        CrossSlot& slot = slots[slot_idx];
        {
            std::unique_lock<std::mutex> lock(slot.mtx);
            slot.cv.wait(lock, [&] { return slot.filled || transfer_error.load(); });
            if (transfer_error.load()) {
                break;
            }
        }

        int rx_actual = 0;
        const bool ok = usb_transfer_chunk(ctx,
                                           send_handle,
                                           recv_handle,
                                           slot.tx_buf.data(),
                                           slot.rx_buf.data(),
                                           slot.tx_bytes,
                                           &rx_actual);
        if (!ok) {
            transfer_error.store(true);
            {
                std::lock_guard<std::mutex> lock(slot.mtx);
                slot.usb_done = true;
            }
            slot.cv.notify_all();
            break;
        }

        all_done = slot.eof;

        {
            std::lock_guard<std::mutex> lock(slot.mtx);
            slot.rx_bytes = rx_actual;
            slot.usb_done = true;
        }
        slot.cv.notify_all();

        total_bytes_done += static_cast<uint64_t>(rx_actual);

        if (progress_cb) {
            progress_cb(total_bytes_done, file_size, elapsed_seconds(started));
        }

        slot_idx = 1 - slot_idx;
    }

    reader_thread.join();
    writer_thread.join();

    out_file.flush();
    out_file.close();
    in_file.close();
    close_device(send_handle);
    close_device(recv_handle);

    const double seconds = elapsed_seconds(started);
    if (transfer_error.load() || total_bytes_done != file_size) {
        std::string message = "Received " + std::to_string(total_bytes_done)
            + " of " + std::to_string(file_size) + " bytes";
        if (transfer_error.load()) {
            message = "USB transfer failed during loopback";
        }
        return make_result(false, total_bytes_done, file_size, seconds, message);
    }

    return make_result(true, total_bytes_done, file_size, seconds);
}

}  // namespace

void set_payload_timeout_ms(unsigned ms) {
    g_payload_timeout_ms.store(ms != 0 ? ms : usb_protocol::kFileTimeoutMs);
}

unsigned payload_timeout_ms() {
    return g_payload_timeout_ms.load();
}

void set_inflight_budget_mb(unsigned mb) {
    g_inflight_budget_mb.store(mb);
}

unsigned inflight_queue_depth() {
    return effective_queue_depth();
}

unsigned usbfs_limit_mb() {
    return read_usbfs_limit_mb();
}

TransferResult send_file_core(libusb_context* ctx,
                              const std::string& path,
                              int port_index,
                              ProgressCallback progress_cb,
                              unsigned timeout_ms,
                              uint8_t frame_kind,
                              const std::string& header_filename) {
    TransferResult result{};

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.error_message = "Could not open file: " + path;
        return result;
    }

    const uint64_t file_size = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    result.expected_bytes = file_size;

    libusb_device_handle* handle = open_device_by_index(ctx, port_index, &result.error_message);
    if (!handle) {
        if (result.error_message.empty()) {
            result.error_message =
                "Could not open USB device at port index " + std::to_string(port_index);
        }
        file.close();
        return result;
    }

    FileHeader hdr{};
    fill_file_header(&hdr,
                     file_size,
                     frame_kind,
                     header_filename.empty() ? nullptr : header_filename.c_str());
    std::string hdr_write_err;
    if (!bulk_write_all(handle,
                        reinterpret_cast<uint8_t*>(&hdr),
                        sizeof(hdr),
                        static_cast<int>(timeout_ms),
                        &hdr_write_err)) {
        result.error_message = "Header send failed: " + hdr_write_err;
        close_device(handle);
        file.close();
        return result;
    }

    std::vector<QueuedXfr> queue(usb_protocol::kQueueDepth);
    for (auto& q : queue) {
        q.xfr = libusb_alloc_transfer(0);
    }

    uint64_t bytes_read = 0;
    uint64_t bytes_sent = 0;
    int active = 0;
    bool disk_done = false;
    bool error = false;
    std::string error_msg;
    const auto t_start = Clock::now();

    const auto submit_next = [&](QueuedXfr& q) -> bool {
        if (disk_done) {
            return false;
        }
        const uint64_t remaining = file_size - bytes_read;
        if (remaining == 0) {
            disk_done = true;
            return false;
        }

        std::memset(q.buf.data(), 0x00, usb_protocol::kChunkSize);

        const int to_read = static_cast<int>(
            std::min(remaining, static_cast<uint64_t>(usb_protocol::kChunkSize)));
        file.read(reinterpret_cast<char*>(q.buf.data()), to_read);
        const int n = static_cast<int>(file.gcount());
        if (n <= 0) {
            error = true;
            error_msg = "Disk read error at byte " + std::to_string(bytes_read);
            return false;
        }

        q.valid_bytes = n;
        q.completed = false;
        q.in_flight = true;

        fprintf(stderr, "[USB-DIAG] async_submit EP=0x%02x wire=%dB timeout=%ums\n",
                usb_protocol::kEndpointDataOut, n,
                static_cast<unsigned>(timeout_ms));
        libusb_fill_bulk_transfer(
            q.xfr,
            handle,
            usb_protocol::kEndpointDataOut,
            q.buf.data(),
            n,
            queued_cb,
            &q,
            static_cast<int>(timeout_ms));
        const int submit_rc = libusb_submit_transfer(q.xfr);
        fprintf(stderr, "[USB-DIAG] async_submit rc=%d (%s)\n",
                submit_rc, libusb_strerror(static_cast<libusb_error>(submit_rc)));
        if (submit_rc != 0) {
            q.in_flight = false;
            error = true;
            error_msg = "USB submit failed, rc=" + std::to_string(submit_rc);
            return false;
        }

        bytes_read += static_cast<uint64_t>(n);
        if (bytes_read >= file_size) {
            disk_done = true;
        }
        ++active;
        return true;
    };

    const unsigned max_inflight = effective_queue_depth();
    for (std::size_t i = 0; i < max_inflight && !disk_done && !error; ++i) {
        submit_next(queue[i]);
    }

    while (active > 0 && !error) {
        timeval tv{0, 1000};
        libusb_handle_events_timeout(ctx, &tv);

        for (auto& q : queue) {
            if (!q.completed) {
                continue;
            }
            q.completed = false;
            --active;

            if (q.status != LIBUSB_TRANSFER_COMPLETED) {
                fprintf(stderr, "[USB-DIAG] async_complete FAILED status=%d "
                        "(0=ok,1=err,2=timeout,3=cancel,4=stall,5=nodev,6=overflow)\n",
                        q.status);
                error = true;
                error_msg = "USB transfer failed, status=" + std::to_string(q.status);
                break;
            }

            bytes_sent += static_cast<uint64_t>(q.valid_bytes);

            if (progress_cb) {
                progress_cb(bytes_sent, file_size, elapsed_seconds(t_start));
            }

            if (!disk_done && !error) {
                submit_next(q);
            }
        }
    }

    if (error) {
        for (auto& q : queue) {
            if (q.in_flight) {
                libusb_cancel_transfer(q.xfr);
            }
        }
        while (active > 0) {
            timeval tv{0, 10000};
            libusb_handle_events_timeout(ctx, &tv);
            for (auto& q : queue) {
                if (q.completed) {
                    q.completed = false;
                    --active;
                }
            }
        }
    }

    for (auto& q : queue) {
        libusb_free_transfer(q.xfr);
    }
    file.close();
    close_device(handle);

    result.seconds = elapsed_seconds(t_start);
    result.bytes_transferred = bytes_sent;
    result.mbps = mib_per_second(bytes_sent, result.seconds);
    result.ok = (!error && bytes_sent == file_size);
    if (!result.ok && error_msg.empty()) {
        error_msg = "Sent " + std::to_string(bytes_sent) + " of "
            + std::to_string(file_size) + " bytes";
    }
    result.error_message = error_msg;
    return result;
}

TransferResult receive_file_core(libusb_context* ctx,
                                 const std::string& out_path,
                                 int port_index,
                                 ProgressCallback progress_cb,
                                 unsigned header_timeout_ms,
                                 uint8_t expected_frame_kind) {
    TransferResult result{};

    libusb_device_handle* handle = open_device_by_index(ctx, port_index, &result.error_message);
    if (!handle) {
        if (result.error_message.empty()) {
            result.error_message =
                "Could not open USB device at port index " + std::to_string(port_index);
        }
        return result;
    }

    std::ofstream out_file(out_path, std::ios::binary | std::ios::trunc);
    if (!out_file.is_open()) {
        result.error_message = "Could not create output file: " + out_path;
        close_device(handle);
        return result;
    }

    FileHeader hdr{};
    const auto header_deadline =
        Clock::now() + std::chrono::milliseconds(header_timeout_ms);
    bool got_payload_header = false;

    while (Clock::now() < header_deadline) {
        const int remaining_ms = remaining_timeout_ms(header_deadline);
        if (!bulk_read_all(handle,
                           reinterpret_cast<uint8_t*>(&hdr),
                           sizeof(hdr),
                           remaining_ms)) {
            break;
        }
        if (std::memcmp(hdr.magic, usb_protocol::kHeaderMagic, 8) != 0) {
            result.error_message = "Bad magic bytes in header";
            out_file.close();
            close_device(handle);
            return result;
        }

        if (expected_frame_kind == usb_protocol::kFrameKindPayload
            && hdr.frame_kind == usb_protocol::kFrameKindSession) {
            fprintf(stderr,
                    "[USB-DIAG] stray session frame during payload wait (%llu B), skipping\n",
                    static_cast<unsigned long long>(hdr.file_size));
            if (!discard_incoming_bytes(handle, hdr.file_size, remaining_timeout_ms(header_deadline))) {
                result.error_message = "Stray session frame read failed";
                out_file.close();
                close_device(handle);
                return result;
            }
            continue;
        }

        if (expected_frame_kind != 0 && hdr.frame_kind != expected_frame_kind) {
            result.error_message = "Unexpected frame kind in header";
            out_file.close();
            close_device(handle);
            return result;
        }

        got_payload_header = true;
        break;
    }

    if (!got_payload_header) {
        result.error_message = "Header read failed";
        out_file.close();
        close_device(handle);
        return result;
    }

    const uint64_t file_size = hdr.file_size;
    result.expected_bytes = file_size;

    std::vector<RingSlot> pool(usb_protocol::kPoolSize);
    for (auto& slot : pool) {
        slot.xfr = libusb_alloc_transfer(0);
    }

    std::atomic<bool> write_error{false};
    std::atomic<bool> usb_done{false};
    std::atomic<bool> recv_aborted{false};
    std::atomic<int> total_submitted{0};

    std::thread writer([&]() {
        int write_idx = 0;
        while (true) {
            const int pool_idx = write_idx % static_cast<int>(usb_protocol::kPoolSize);
            RingSlot& slot = pool[static_cast<std::size_t>(pool_idx)];

            while (slot.state.load(std::memory_order_acquire) != BufState::READY) {
                if (recv_aborted.load()) {
                    return;
                }
                if (usb_done.load() && write_idx >= total_submitted.load()) {
                    return;
                }
                std::this_thread::yield();
            }

            std::atomic_thread_fence(std::memory_order_acquire);

            if (slot.real_bytes > 0) {
                out_file.write(reinterpret_cast<const char*>(slot.buf.data()),
                               slot.real_bytes);
                if (!out_file.good()) {
                    write_error.store(true);
                    slot.state.store(BufState::FREE, std::memory_order_release);
                    return;
                }
            }

            slot.state.store(BufState::FREE, std::memory_order_release);
            ++write_idx;
        }
    });

    uint64_t bytes_submitted = 0;
    uint64_t bytes_received = 0;
    int active = 0;
    bool error = false;
    std::string error_msg;
    int submit_count = 0;
    const auto t_start = Clock::now();

    const int max_inflight = static_cast<int>(effective_queue_depth());
    const auto try_submit = [&]() {
        while (active < max_inflight
               && bytes_submitted < file_size && !error) {
            const int pool_idx = submit_count % static_cast<int>(usb_protocol::kPoolSize);
            RingSlot& slot = pool[static_cast<std::size_t>(pool_idx)];

            while (slot.state.load(std::memory_order_acquire) != BufState::FREE) {
                if (write_error.load()) {
                    error = true;
                    return;
                }
                std::this_thread::yield();
            }

            const uint64_t remaining = file_size - bytes_submitted;
            slot.real_bytes = static_cast<int>(
                std::min(remaining, static_cast<uint64_t>(usb_protocol::kChunkSize)));

            if (static_cast<uint64_t>(slot.real_bytes) < usb_protocol::kChunkSize) {
                std::memset(slot.buf.data(), 0x00, usb_protocol::kChunkSize);
            }

            slot.status = 0;
            slot.state.store(BufState::IN_FLIGHT, std::memory_order_release);

            libusb_fill_bulk_transfer(
                slot.xfr,
                handle,
                usb_protocol::kEndpointDataIn,
                slot.buf.data(),
                static_cast<int>(usb_protocol::kChunkSize),
                ring_cb,
                &slot,
                static_cast<int>(payload_timeout_ms()));
            const int submit_rc = libusb_submit_transfer(slot.xfr);
            if (submit_rc != 0) {
                // Device gone (e.g. cable pulled): the slot will never get a
                // completion callback, so don't leave it IN_FLIGHT or the loop
                // below spins forever. Fail the transfer immediately.
                slot.state.store(BufState::FREE, std::memory_order_release);
                error = true;
                error_msg = "USB submit failed, rc=" + std::to_string(submit_rc);
                return;
            }

            bytes_submitted += static_cast<uint64_t>(slot.real_bytes);
            ++submit_count;
            total_submitted.store(submit_count, std::memory_order_release);
            ++active;
        }
    };

    try_submit();

    int next_complete = 0;
    while (bytes_received < file_size && !error && !write_error.load()) {
        const int pool_idx = next_complete % static_cast<int>(usb_protocol::kPoolSize);
        RingSlot& slot = pool[static_cast<std::size_t>(pool_idx)];

        while (slot.state.load(std::memory_order_acquire) == BufState::IN_FLIGHT) {
            if (write_error.load()) {
                error = true;
                break;
            }
            timeval tv{0, 1000};
            libusb_handle_events_timeout(ctx, &tv);
        }
        if (error || write_error.load()) {
            break;
        }

        std::atomic_thread_fence(std::memory_order_acquire);

        if (slot.status != LIBUSB_TRANSFER_COMPLETED) {
            // Cable pulled / device error mid-stream: libusb completes the
            // in-flight transfer with a non-OK status (NO_DEVICE, TIMED_OUT, …).
            // Abort instead of treating it as a 0-byte success and looping.
            error = true;
            error_msg = "USB receive failed, status=" + std::to_string(slot.status);
            slot.state.store(BufState::FREE, std::memory_order_release);
            break;
        }

        const uint64_t remaining = file_size - bytes_received;
        const int real_bytes = static_cast<int>(std::min(
            static_cast<uint64_t>(slot.actual), remaining));
        slot.real_bytes = real_bytes;

        bytes_received += static_cast<uint64_t>(real_bytes);

        if (progress_cb) {
            progress_cb(bytes_received, file_size, elapsed_seconds(t_start));
        }

        --active;
        ++next_complete;

        try_submit();
    }

    if (error || write_error.load()) {
        // The writer may be parked waiting on a slot that errored and will never
        // go READY; release it before draining/joining.
        recv_aborted.store(true, std::memory_order_release);

        for (auto& slot : pool) {
            if (slot.state.load() == BufState::IN_FLIGHT) {
                libusb_cancel_transfer(slot.xfr);
            }
        }
        // Bounded drain: if the device is gone, cancelled transfers complete
        // quickly; cap the wait so a wedged device can never hang us here.
        const auto drain_start = Clock::now();
        while (elapsed_seconds(drain_start) < 2.0) {
            bool any_inflight = false;
            for (auto& slot : pool) {
                if (slot.state.load() == BufState::IN_FLIGHT) {
                    any_inflight = true;
                    break;
                }
            }
            if (!any_inflight) {
                break;
            }
            timeval tv{0, 10000};
            libusb_handle_events_timeout(ctx, &tv);
        }
    }

    usb_done.store(true, std::memory_order_release);
    writer.join();

    for (auto& slot : pool) {
        libusb_free_transfer(slot.xfr);
    }
    out_file.flush();
    out_file.close();
    close_device(handle);

    result.seconds = elapsed_seconds(t_start);
    result.bytes_transferred = bytes_received;
    result.mbps = mib_per_second(bytes_received, result.seconds);
    result.ok = (!error && !write_error.load() && bytes_received == file_size);
    if (!result.ok) {
        if (write_error.load()) {
            error_msg = "Disk write error";
        }
        if (error_msg.empty()) {
            error_msg = "Received " + std::to_string(bytes_received) + " of "
                + std::to_string(file_size) + " bytes";
        }
    }
    result.error_message = error_msg;
    return result;
}

std::vector<FabricUsbDevice> list_fabric_devices(libusb_context* ctx) {
    std::vector<FabricUsbDevice> devices;
    libusb_device** list = nullptr;
    const ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        return devices;
    }

    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        if (desc.idVendor != usb_protocol::kVendorId
            || desc.idProduct != usb_protocol::kProductId) {
            continue;
        }
        devices.push_back({
            libusb_get_bus_number(list[i]),
            libusb_get_device_address(list[i]),
        });
    }

    libusb_free_device_list(list, 1);
    std::sort(devices.begin(), devices.end(),
              [](const FabricUsbDevice& a, const FabricUsbDevice& b) {
                  if (a.bus != b.bus) {
                      return a.bus < b.bus;
                  }
                  return a.addr < b.addr;
              });
    return devices;
}

int count_fabric_devices(libusb_context* ctx) {
    return static_cast<int>(list_fabric_devices(ctx).size());
}

bool fabric_port_available(libusb_context* ctx, int port_index) {
    if (!ctx || port_index < 0) {
        return false;
    }
    std::string error;
    libusb_device_handle* handle =
        open_device_by_index(ctx, port_index, &error, /*max_open_attempts=*/1);
    if (!handle) {
        return false;
    }
    libusb_release_interface(handle, usb_protocol::kInterface);
    libusb_close(handle);
    return true;
}

int resolve_fabric_port_index(libusb_context* ctx) {
    const std::vector<FabricUsbDevice> devices = list_fabric_devices(ctx);
    if (devices.empty()) {
        return -1;
    }
    if (devices.size() == 1) {
        return 0;
    }
    for (int index = 0; index < static_cast<int>(devices.size()); ++index) {
        if (fabric_port_available(ctx, index)) {
            return index;
        }
    }
    return 0;
}

bool fabric_device_bus_addr(libusb_context* ctx,
                            int port_index,
                            uint8_t* bus_out,
                            uint8_t* addr_out) {
    libusb_device** list = nullptr;
    const ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        return false;
    }

    int found = 0;
    bool ok = false;

    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        if (desc.idVendor != usb_protocol::kVendorId
            || desc.idProduct != usb_protocol::kProductId) {
            continue;
        }
        if (found == port_index) {
            if (bus_out) {
                *bus_out = libusb_get_bus_number(list[i]);
            }
            if (addr_out) {
                *addr_out = libusb_get_device_address(list[i]);
            }
            ok = true;
            break;
        }
        ++found;
    }

    libusb_free_device_list(list, 1);
    return ok;
}

std::string fabric_device_serial(libusb_context* ctx, int port_index) {
    if (!ctx || port_index < 0) {
        return {};
    }

    libusb_device** list = nullptr;
    const ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        return {};
    }

    int found = 0;
    libusb_device* target_dev = nullptr;
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        if (desc.idVendor != usb_protocol::kVendorId
            || desc.idProduct != usb_protocol::kProductId) {
            continue;
        }
        if (found == port_index) {
            target_dev = list[i];
            break;
        }
        ++found;
    }

    if (!target_dev) {
        libusb_free_device_list(list, 1);
        return {};
    }

    libusb_device_handle* handle = nullptr;
    std::string serial;
    if (libusb_open(target_dev, &handle) == LIBUSB_SUCCESS) {
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(target_dev, &desc) == 0 && desc.iSerialNumber != 0) {
            unsigned char buffer[256] = {};
            const int len = libusb_get_string_descriptor_ascii(
                handle, desc.iSerialNumber, buffer, sizeof(buffer));
            if (len > 0) {
                serial.assign(reinterpret_cast<char*>(buffer),
                              reinterpret_cast<char*>(buffer + len));
            }
        }
        libusb_close(handle);
    }

    libusb_free_device_list(list, 1);
    return serial;
}

TransferResult loopback_transfer_core(libusb_context* ctx,
                                      const std::string& path,
                                      int send_port_index,
                                      int recv_port_index,
                                      ProgressCallback progress_cb) {
    const auto started = Clock::now();

    if (send_port_index == recv_port_index) {
        return make_result(false, 0, 0, 0.0,
                           "Loopback requires two different port indexes");
    }

    const int devices = count_fabric_devices(ctx);
    if (devices < 2) {
        return make_result(
            false,
            0,
            0,
            0.0,
            "Loopback needs 2 USB cables from this PC to two CON ports (found "
                + std::to_string(devices)
                + "). Use a USB-C hub if the laptop has one port.");
    }

    const std::string recv_path = path + ".loopback_recv";
    const TransferResult cross = file_cross_connect_core(
        ctx, path, recv_path, send_port_index, recv_port_index, progress_cb);

    if (!cross.ok) {
        std::remove(recv_path.c_str());
        return cross;
    }

    if (!files_equal(path, recv_path)) {
        std::remove(recv_path.c_str());
        return make_result(false,
                           cross.bytes_transferred,
                           cross.expected_bytes,
                           elapsed_seconds(started),
                           "Loopback data mismatch");
    }

    std::remove(recv_path.c_str());
    return cross;
}
