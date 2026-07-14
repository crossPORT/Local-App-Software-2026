#include "rocketbox_ping.hpp"
#include "dialer.hpp"
#include "icmp_ping.hpp"

#include <chrono>
#include <iostream>

int run_rocketbox_ping(rocketbox::RocketBoxTransport& transport, int local_port, int peer_port) {
  CircuitDialer dialer(transport, local_port);

  std::cerr << "[rocketbox-tunnel] ping: local=10.64.0." << local_port << " peer=10.64.0."
            << peer_port << " (peer must run full tunnel bridge)\n";
  if (!dialer.ensure(peer_port)) {
    std::cerr << "[rocketbox-tunnel] ping: dial failed port " << peer_port << std::endl;
    return 1;
  }

  for (uint16_t seq = 1; seq <= 4; ++seq) {
    auto pkt = rocketbox_icmp::echo_request(local_port, peer_port, 0x5242, seq);
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<uint8_t> reply;
    // Atomic send+recv on the stream handle — avoids listen-thread races.
    const bool ok = dialer.exchange_message(pkt, &reply, 2000);
    const auto ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::cerr << "[rocketbox-tunnel] ping: seq=" << seq << " sent bytes=" << pkt.size() << std::endl;
    if (ok && rocketbox_icmp::is_echo_reply(reply.data(), reply.size(), 0x5242)) {
      std::cout << "rocketbox ping 10.64.0." << peer_port << ": seq=" << seq << " time=" << ms
                << " ms\n";
    } else if (ok) {
      std::cout << "rocketbox ping 10.64.0." << peer_port << ": seq=" << seq
                << " timeout (inbound bytes=" << reply.size() << " not echo-reply)\n";
    } else {
      std::cout << "rocketbox ping 10.64.0." << peer_port << ": seq=" << seq
                << " timeout (no USB inbound)\n";
    }
    dialer.note_activity();
  }
  dialer.shutdown();
  return 0;
}
