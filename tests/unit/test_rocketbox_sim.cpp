#include "test_util.h"

#include "rocketbox_sim.h"

#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

std::string write_temp_file(const std::string& contents) {
    char tmpl[] = "/tmp/rocketbox-sim-test-XXXXXX";
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

RB_TEST(rocketbox_sim_count_and_ports) {
    rocketbox_sim_set_enabled(true);
    rocketbox_sim_reset();
    CHECK(rocketbox_sim_count_devices() == 2);
    CHECK(rocketbox_sim_port_available(0));
    CHECK(rocketbox_sim_port_available(1));
    CHECK(!rocketbox_sim_port_available(2));
    rocketbox_sim_set_enabled(false);
    rocketbox_sim_reset();
}

RB_TEST(rocketbox_sim_port0_to_port1_transfer) {
    rocketbox_sim_set_enabled(true);
    rocketbox_sim_reset();

    const std::string payload(4096, 'L');
    const std::string src = write_temp_file(payload);
    CHECK(!src.empty());

    char recv_path[] = "/tmp/rocketbox-sim-recv-XXXXXX";
    const int recv_fd = mkstemp(recv_path);
    CHECK(recv_fd >= 0);
    ::close(recv_fd);

    TransferResult recv_result{};
    std::thread receiver([&]() {
        recv_result = rocketbox_sim_receive_file(
            recv_path, 1, nullptr, usb_protocol::kSessionHeaderTimeoutMs);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const TransferResult send_result =
        rocketbox_sim_send_file(src, 0, nullptr, usb_protocol::kFileTimeoutMs);
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
    rocketbox_sim_set_enabled(false);
    rocketbox_sim_reset();
}

RB_TEST(rocketbox_sim_loopback_transfer) {
    rocketbox_sim_set_enabled(true);
    rocketbox_sim_reset();

    const std::string payload(4096, 'L');
    const std::string src = write_temp_file(payload);
    CHECK(!src.empty());

    const TransferResult result = rocketbox_sim_loopback(src, 0, 1, nullptr);
    CHECK(result.ok);
    CHECK(result.bytes_transferred == payload.size());

    std::remove(src.c_str());
    rocketbox_sim_set_enabled(false);
    rocketbox_sim_reset();
}
