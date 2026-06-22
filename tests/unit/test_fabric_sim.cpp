#include "test_util.h"

#include "fabric_sim.h"

#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

std::string write_temp_file(const std::string& contents) {
    char tmpl[] = "/tmp/slsfabric-sim-test-XXXXXX";
    const int fd = mkstemp(tmpl);
    if (fd < 0) {
        return {};
    }
    const ssize_t written = ::write(fd, contents.data(), contents.size());
    ::close(fd);
    if (written < 0 || static_cast<std::size_t>(written) != contents.size()) {
        std::remove(tmpl);
        return {};
    }
    return tmpl;
}

}  // namespace

FABRIC_TEST(fabric_sim_count_and_ports) {
    fabric_sim_set_enabled(true);
    fabric_sim_reset();
    CHECK(fabric_sim_count_devices() == 2);
    CHECK(fabric_sim_port_available(0));
    CHECK(fabric_sim_port_available(1));
    CHECK(!fabric_sim_port_available(2));
    fabric_sim_set_enabled(false);
    fabric_sim_reset();
}

FABRIC_TEST(fabric_sim_port0_to_port1_transfer) {
    fabric_sim_set_enabled(true);
    fabric_sim_reset();

    const std::string payload(4096, 'L');
    const std::string src = write_temp_file(payload);
    CHECK(!src.empty());

    char recv_path[] = "/tmp/slsfabric-sim-recv-XXXXXX";
    const int recv_fd = mkstemp(recv_path);
    CHECK(recv_fd >= 0);
    ::close(recv_fd);

    TransferResult recv_result{};
    std::thread receiver([&]() {
        recv_result = fabric_sim_receive_file(
            recv_path, 1, nullptr, usb_protocol::kSessionHeaderTimeoutMs);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const TransferResult send_result =
        fabric_sim_send_file(src, 0, nullptr, usb_protocol::kFileTimeoutMs);
    receiver.join();

    CHECK(send_result.ok);
    CHECK(recv_result.ok);
    CHECK(send_result.bytes_transferred == payload.size());
    CHECK(recv_result.bytes_transferred == payload.size());

    std::ifstream in(recv_path, std::ios::binary);
    std::string got((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(got == payload);

    std::remove(src.c_str());
    std::remove(recv_path);
    fabric_sim_set_enabled(false);
    fabric_sim_reset();
}

FABRIC_TEST(fabric_sim_loopback_transfer) {
    fabric_sim_set_enabled(true);
    fabric_sim_reset();

    const std::string payload(4096, 'L');
    const std::string src = write_temp_file(payload);
    CHECK(!src.empty());

    const TransferResult result = fabric_sim_loopback(src, 0, 1, nullptr);
    CHECK(result.ok);
    CHECK(result.bytes_transferred == payload.size());

    std::remove(src.c_str());
    fabric_sim_set_enabled(false);
    fabric_sim_reset();
}
