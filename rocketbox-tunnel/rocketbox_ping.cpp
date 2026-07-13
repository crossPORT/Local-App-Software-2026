#include "rocketbox_ping.hpp"
#include "dialer.hpp"
#include "icmp_ping.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

int run_rocketbox_ping(rocketbox::RocketBoxTransport& transport, int local_port, int peer_port) {
    CircuitDialer dialer(transport, local_port);
    std::atomic<bool> got{false};
    dialer.on_message([&](const std::vector<uint8_t>& msg) {
        if (rocketbox_icmp::is_echo_reply(msg.data(), msg.size(), 0x5242)) {
            got = true;
        }
    });

    if (!dialer.ensure(peer_port)) {
        std::cerr << "[rocketbox-tunnel] ping: dial failed port " << peer_port << std::endl;
        return 1;
    }

    for (uint16_t seq = 1; seq <= 4; ++seq) {
        got = false;
        auto pkt = rocketbox_icmp::echo_request(local_port, peer_port, 0x5242, seq);
        const auto t0 = std::chrono::steady_clock::now();
        dialer.send_message(pkt);
        for (int i = 0; i < 40 && !got; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        const auto ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (got) {
            std::cout << "rocketbox ping 10.64.0." << peer_port << ": seq=" << seq << " time=" << ms
                      << " ms\n";
        } else {
            std::cout << "rocketbox ping 10.64.0." << peer_port << ": seq=" << seq << " timeout\n";
        }
        dialer.note_activity();
    }
    dialer.shutdown();
    return 0;
}
