#include "fabric_ping.hpp"
#include "dialer.hpp"
#include "icmp_ping.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
std::atomic<bool> g_ping_stop{false};
}

int run_fabric_ping(rocketbox::Session& session, int local_port, int peer_port) {
  CircuitDialer dialer(session, local_port);
  std::atomic<bool> got{false};
  auto on_msg = [&](const std::vector<uint8_t>& msg) {
    if (fabric_icmp::is_echo_reply(msg.data(), msg.size(), 0x5242)) {
      got = true;
    }
  };
  dialer.on_connection([&](std::shared_ptr<rocketbox::Connection> c) {
    if (c) {
      c->OnMessageReceived(on_msg);
    }
  });

  auto conn = dialer.ensure(peer_port);
  if (!conn) {
    std::cerr << "[rocketbox-tunnel] ping: dial failed port " << peer_port << std::endl;
    return 1;
  }
  conn->OnMessageReceived(on_msg);

  for (uint16_t seq = 1; seq <= 4 && !g_ping_stop; ++seq) {
    got = false;
    auto pkt = fabric_icmp::echo_request(local_port, peer_port, 0x5242, seq);
    const auto t0 = std::chrono::steady_clock::now();
    conn->SendMessage(pkt);
    for (int i = 0; i < 40 && !got; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    const auto ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (got) {
      std::cout << "fabric ping 10.64.0." << peer_port << ": seq=" << seq << " time=" << ms << " ms\n";
    } else {
      std::cout << "fabric ping 10.64.0." << peer_port << ": seq=" << seq << " timeout\n";
    }
    dialer.note_activity();
  }
  dialer.shutdown();
  return 0;
}
