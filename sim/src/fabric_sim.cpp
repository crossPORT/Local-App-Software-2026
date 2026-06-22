#include "fabric_sim.h"

#include "usb_protocol.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct FileHeader {
    uint8_t  magic[8];
    uint64_t file_size;
    uint8_t  reserved[16];
};

static_assert(sizeof(FileHeader) == usb_protocol::kHeaderSize);

class SimChannel {
public:
    void push(const uint8_t* data, std::size_t len) {
        std::lock_guard<std::mutex> lock(mu_);
        buf_.insert(buf_.end(), data, data + len);
        cv_.notify_all();
    }

    bool read(uint8_t* out,
              std::size_t len,
              std::chrono::milliseconds timeout,
              std::string* error_out) {
        std::unique_lock<std::mutex> lock(mu_);
        const auto deadline = Clock::now() + timeout;
        std::size_t got = 0;
        while (got < len) {
            if (!buf_.empty()) {
                const std::size_t take = std::min(len - got, buf_.size());
                std::memcpy(out + got, buf_.data(), take);
                buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(take));
                got += take;
                continue;
            }
            if (timeout.count() == 0) {
                break;
            }
            if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                break;
            }
        }
        if (got < len) {
            if (error_out) {
                *error_out = "Header read failed";
            }
            return false;
        }
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mu_);
        buf_.clear();
    }

private:
    std::mutex mu_;
    std::condition_variable cv_;
    std::vector<uint8_t> buf_;
};

class FabricSimHub {
public:
    static FabricSimHub& instance() {
        static FabricSimHub hub;
        return hub;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(route_mu_);
        rx_[0].clear();
        rx_[1].clear();
    }

    void transmit(int from_port, const uint8_t* data, std::size_t len) {
        if (from_port < 0 || from_port > 1 || len == 0) {
            return;
        }
        const int to_port = 1 - from_port;
        std::lock_guard<std::mutex> lock(route_mu_);
        rx_[to_port].push(data, len);
    }

    SimChannel& rx(int port_index) {
        return rx_[port_index == 0 ? 0 : 1];
    }

private:
    FabricSimHub() = default;

    std::mutex route_mu_;
    SimChannel rx_[2];
};

bool sim_enabled_flag() {
    static int cached = -1;
    if (cached < 0) {
        const char* env = std::getenv("ROCKETBOX_SIM");
        cached = (env != nullptr && env[0] != '\0' && env[0] != '0') ? 1 : 0;
    }
    return cached != 0;
}

static int g_sim_override = -1;

double elapsed_seconds(const Clock::time_point& start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

double mib_per_second(uint64_t bytes, double seconds) {
    if (bytes == 0) {
        return 0.0;
    }
    const double effective_seconds = std::max(seconds, 0.001);
    return static_cast<double>(bytes) / (1024.0 * 1024.0) / effective_seconds;
}

TransferResult make_ok(uint64_t bytes, uint64_t expected, const Clock::time_point& start) {
    TransferResult result{};
    result.ok = true;
    result.bytes_transferred = bytes;
    result.expected_bytes = expected;
    result.seconds = elapsed_seconds(start);
    result.mbps = mib_per_second(bytes, result.seconds);
    return result;
}

TransferResult make_fail(const std::string& message) {
    TransferResult result{};
    result.error_message = message;
    return result;
}

bool valid_port(int port_index, std::string* error_out) {
    if (port_index < 0 || port_index > 1) {
        if (error_out) {
            *error_out = "Sim fabric supports port index 0 and 1 only";
        }
        return false;
    }
    return true;
}

void fill_header(FileHeader* hdr, uint64_t file_size) {
    std::memcpy(hdr->magic, usb_protocol::kHeaderMagic, 8);
    hdr->file_size = file_size;
    std::memset(hdr->reserved, 0, sizeof(hdr->reserved));
}

}  // namespace

bool fabric_sim_enabled() {
    if (g_sim_override >= 0) {
        return g_sim_override != 0;
    }
    return sim_enabled_flag();
}

void fabric_sim_set_enabled(bool enabled) {
    g_sim_override = enabled ? 1 : 0;
}

void fabric_sim_reset() {
    FabricSimHub::instance().reset();
}

int fabric_sim_count_devices() {
    return 2;
}

bool fabric_sim_port_available(int port_index) {
    return port_index == 0 || port_index == 1;
}

bool fabric_sim_device_bus_addr(int port_index, uint8_t* bus_out, uint8_t* addr_out) {
    if (!fabric_sim_port_available(port_index)) {
        return false;
    }
    if (bus_out) {
        *bus_out = static_cast<uint8_t>(port_index + 1);
    }
    if (addr_out) {
        *addr_out = static_cast<uint8_t>(port_index + 10);
    }
    return true;
}

std::vector<FabricUsbDevice> fabric_sim_list_devices() {
    return {
        FabricUsbDevice{1, 10},
        FabricUsbDevice{2, 11},
    };
}

TransferResult fabric_sim_send_file(const std::string& path,
                                    int port_index,
                                    ProgressCallback progress_cb,
                                    unsigned /*timeout_ms*/) {
    std::string port_error;
    if (!valid_port(port_index, &port_error)) {
        return make_fail(port_error);
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return make_fail("Could not open file: " + path);
    }

    const uint64_t file_size = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    FileHeader hdr{};
    fill_header(&hdr, file_size);
    FabricSimHub::instance().transmit(
        port_index, reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr));

    const auto t_start = Clock::now();
    uint64_t sent = 0;
    std::vector<uint8_t> chunk(static_cast<std::size_t>(
        std::min(file_size, static_cast<uint64_t>(usb_protocol::kChunkSize))));
    while (sent < file_size) {
        const uint64_t remaining = file_size - sent;
        const std::size_t to_read =
            static_cast<std::size_t>(std::min(remaining, static_cast<uint64_t>(usb_protocol::kChunkSize)));
        file.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(to_read));
        const std::streamsize n = file.gcount();
        if (n <= 0) {
            return make_fail("Disk read error at byte " + std::to_string(sent));
        }
        FabricSimHub::instance().transmit(port_index, chunk.data(), static_cast<std::size_t>(n));
        sent += static_cast<uint64_t>(n);
        if (progress_cb) {
            progress_cb(sent, file_size, elapsed_seconds(t_start));
        }
    }

    return make_ok(sent, file_size, t_start);
}

TransferResult fabric_sim_receive_file(const std::string& out_path,
                                       int port_index,
                                       ProgressCallback progress_cb,
                                       unsigned header_timeout_ms) {
    std::string port_error;
    if (!valid_port(port_index, &port_error)) {
        return make_fail(port_error);
    }

    std::ofstream out_file(out_path, std::ios::binary | std::ios::trunc);
    if (!out_file.is_open()) {
        return make_fail("Could not create output file: " + out_path);
    }

    FileHeader hdr{};
    std::string read_error;
    const auto header_timeout = std::chrono::milliseconds(header_timeout_ms);
    if (!FabricSimHub::instance().rx(port_index).read(
            reinterpret_cast<uint8_t*>(&hdr),
            sizeof(hdr),
            header_timeout,
            &read_error)) {
        return make_fail(read_error.empty() ? "Header read failed" : read_error);
    }

    if (std::memcmp(hdr.magic, usb_protocol::kHeaderMagic, 8) != 0) {
        return make_fail("Bad magic bytes in header");
    }

    const uint64_t file_size = hdr.file_size;
    const auto t_start = Clock::now();
    uint64_t received = 0;
    while (received < file_size) {
        const uint64_t remaining = file_size - received;
        const std::size_t want =
            static_cast<std::size_t>(std::min(remaining, static_cast<uint64_t>(usb_protocol::kChunkSize)));
        std::vector<uint8_t> chunk(want);
        if (!FabricSimHub::instance().rx(port_index).read(
                chunk.data(), want, std::chrono::milliseconds(usb_protocol::kFileTimeoutMs), &read_error)) {
            return make_fail(read_error.empty()
                                 ? "Payload read stalled at byte " + std::to_string(received)
                                 : read_error);
        }
        out_file.write(reinterpret_cast<const char*>(chunk.data()), static_cast<std::streamsize>(want));
        if (!out_file.good()) {
            return make_fail("Disk write error");
        }
        received += want;
        if (progress_cb) {
            progress_cb(received, file_size, elapsed_seconds(t_start));
        }
    }

    TransferResult result = make_ok(received, file_size, t_start);
    if (received != file_size) {
        result.ok = false;
        result.error_message = "Short read";
    }
    return result;
}

TransferResult fabric_sim_loopback(const std::string& path,
                                   int send_port_index,
                                   int recv_port_index,
                                   ProgressCallback progress_cb) {
    if (send_port_index == recv_port_index) {
        return make_fail("Loopback requires two different port indexes");
    }
    std::string port_error;
    if (!valid_port(send_port_index, &port_error) || !valid_port(recv_port_index, &port_error)) {
        return make_fail(port_error);
    }

    const std::string recv_path = path + ".loopback_recv";
    std::atomic<bool> recv_waiting{false};
    TransferResult recv_result{};
    TransferResult send_result{};

    std::thread receiver([&]() {
        recv_waiting.store(true, std::memory_order_release);
        recv_result = fabric_sim_receive_file(
            recv_path, recv_port_index, progress_cb, usb_protocol::kFileTimeoutMs);
    });

    while (!recv_waiting.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    send_result = fabric_sim_send_file(path, send_port_index, progress_cb, usb_protocol::kFileTimeoutMs);
    receiver.join();

    if (!send_result.ok) {
        std::remove(recv_path.c_str());
        return send_result;
    }
    if (!recv_result.ok) {
        std::remove(recv_path.c_str());
        return recv_result;
    }

    std::ifstream a(path, std::ios::binary);
    std::ifstream b(recv_path, std::ios::binary);
    const bool equal = std::equal(
        std::istreambuf_iterator<char>(a),
        std::istreambuf_iterator<char>(),
        std::istreambuf_iterator<char>(b));
    std::remove(recv_path.c_str());
    if (!equal) {
        return make_fail("Loopback data mismatch");
    }
    return send_result;
}
