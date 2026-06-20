#include "test_util.h"
#include "hw_fixture.h"

#include "usb_protocol.h"
#include "usb_transfer.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

constexpr int kRecvPort = 0;
constexpr int kSendPort = 1;

std::string make_random_file(std::size_t size) {
    char tmpl[] = "/tmp/slsfabric-hwtest-XXXXXX";
    const int fd = mkstemp(tmpl);
    if (fd < 0) {
        return {};
    }
    ::close(fd);

    std::ofstream out(tmpl, std::ios::binary | std::ios::trunc);
    std::mt19937 rng(0xC0FFEEu ^ static_cast<unsigned>(size));
    std::vector<char> buf(64 * 1024);
    std::size_t written = 0;
    while (written < size) {
        const std::size_t n = std::min(buf.size(), size - written);
        for (std::size_t i = 0; i < n; ++i) {
            buf[i] = static_cast<char>(rng() & 0xFF);
        }
        out.write(buf.data(), static_cast<std::streamsize>(n));
        written += n;
    }
    out.close();
    return tmpl;
}

bool files_equal(const std::string& a, const std::string& b) {
    std::ifstream l(a, std::ios::binary);
    std::ifstream r(b, std::ios::binary);
    if (!l || !r) {
        return false;
    }
    l.seekg(0, std::ios::end);
    r.seekg(0, std::ios::end);
    if (l.tellg() != r.tellg()) {
        return false;
    }
    l.seekg(0);
    r.seekg(0);
    std::vector<char> la(64 * 1024);
    std::vector<char> ra(64 * 1024);
    while (l && r) {
        l.read(la.data(), static_cast<std::streamsize>(la.size()));
        r.read(ra.data(), static_cast<std::streamsize>(ra.size()));
        if (l.gcount() != r.gcount()) {
            return false;
        }
        if (l.gcount() > 0
            && std::memcmp(la.data(), ra.data(),
                           static_cast<std::size_t>(l.gcount())) != 0) {
            return false;
        }
    }
    return l.eof() && r.eof();
}

// Runs a concurrent send (port 1) + receive (port 0) of `size` bytes and
// returns true only when both legs succeed and the bytes match exactly.
bool round_trip(std::size_t size, std::string* detail) {
    const std::string src = make_random_file(size);
    if (src.empty()) {
        if (detail) *detail = "could not create source file";
        return false;
    }
    const std::string dst = src + ".recv";

    TransferResult recv_result{};
    TransferResult send_result{};

    std::thread recv_thread([&]() {
        recv_result = receive_file_core(g_hw_ctx, dst, kRecvPort, nullptr);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    send_result = send_file_core(g_hw_ctx, src, kSendPort, nullptr);
    recv_thread.join();

    bool ok = send_result.ok && recv_result.ok;
    if (ok) {
        ok = (recv_result.bytes_transferred == size) && files_equal(src, dst);
    }
    if (!ok && detail) {
        *detail = "send_ok=" + std::to_string(send_result.ok)
                + " send_err=" + send_result.error_message
                + " recv_ok=" + std::to_string(recv_result.ok)
                + " recv_err=" + recv_result.error_message
                + " recv_bytes=" + std::to_string(recv_result.bytes_transferred);
    }

    std::remove(src.c_str());
    std::remove(dst.c_str());
    return ok;
}

}  // namespace

FABRIC_TEST(hw_round_trip_tiny) {
    std::string detail;
    const bool ok = round_trip(4096, &detail);
    if (!ok) std::cerr << "    " << detail << "\n";
    CHECK(ok);
}

FABRIC_TEST(hw_round_trip_one_mb) {
    std::string detail;
    const bool ok = round_trip(1024 * 1024, &detail);
    if (!ok) std::cerr << "    " << detail << "\n";
    CHECK(ok);
}

FABRIC_TEST(hw_round_trip_chunk_boundary) {
    std::string detail;
    const bool ok = round_trip(usb_protocol::kChunkSize, &detail);
    if (!ok) std::cerr << "    " << detail << "\n";
    CHECK(ok);
}

FABRIC_TEST(hw_round_trip_chunk_boundary_plus_one) {
    std::string detail;
    const bool ok = round_trip(usb_protocol::kChunkSize + 7, &detail);
    if (!ok) std::cerr << "    " << detail << "\n";
    CHECK(ok);
}

// Exceeds the default 16 MB usbfs pool many times over: proves the in-flight
// auto-clamp keeps large transfers working (no LIBUSB_ERROR_NO_MEM / hang).
FABRIC_TEST(hw_round_trip_exceeds_usbfs_limit) {
    std::string detail;
    const bool ok = round_trip(64ull * 1024 * 1024, &detail);
    if (!ok) std::cerr << "    " << detail << "\n";
    CHECK(ok);
}
