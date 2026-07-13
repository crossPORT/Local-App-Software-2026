#include "bridge.hpp"
#include "dialer.hpp"
#include "expose_spec.hpp"
#include "open_transport.hpp"
#include "peer_map.hpp"
#include "port_lock.hpp"
#include "rocketbox_ping.hpp"
#include "tun_device.hpp"
#include "tunnel_log.hpp"
#include "tunnel_stats.hpp"

#include "rocketbox/sdk.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

struct Options {
  int port = 0;
  int ping_peer = 0;
  std::string iface;
#if defined(__linux__)
  bool use_netns = true;
#else
  bool use_netns = false;
#endif
  rocketbox::TransportMode transport = rocketbox::TransportMode::Usb;
  std::vector<ExposeRule> expose;
};

const char* transport_name(rocketbox::TransportMode t) {
  return t == rocketbox::TransportMode::Usb ? "usb" : "sim";
}

void usage(const char* argv0) {
  std::cerr << "RocketBox tunnel (USB ↔ host IP)\n"
            << "Usage: " << argv0 << " [options]\n"
            << "  (USB default)      Port from cable serial → 10.64.0.N\n"
            << "  --port N           USB: pick among multiple cables; sim: required\n"
            << "  --transport T      usb (default) or sim\n"
            << "  --expose SPEC      Publish host ports: 445, tcp:22, udp:53\n"
            << "  --iface NAME       TUN interface name (default: rbN)\n"
            << "  --no-netns         Keep TUN in the host network namespace\n"
            << "  --ping M           ICMP echo to peer port M, then exit\n";
}

bool parse_args(int argc, char** argv, Options& out) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto need = [&](const char* name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error(std::string("missing value for ") + name);
      }
      return argv[++i];
    };
    if (a == "--port") {
      out.port = std::stoi(need("--port"));
    } else if (a == "--transport") {
      const std::string t = need("--transport");
      if (t == "usb") {
        out.transport = rocketbox::TransportMode::Usb;
      } else if (t == "sim") {
        out.transport = rocketbox::TransportMode::Sim;
      } else {
        throw std::runtime_error("--transport must be usb or sim");
      }
    } else if (a == "--ping") {
      out.ping_peer = std::stoi(need("--ping"));
    } else if (a == "--expose") {
      parse_expose_list(need("--expose"), out.expose);
    } else if (a == "--iface") {
      out.iface = need("--iface");
    } else if (a == "--no-netns") {
      out.use_netns = false;
    } else if (a == "-h" || a == "--help") {
      usage(argv[0]);
      return false;
    } else {
      throw std::runtime_error("unknown arg: " + a);
    }
  }
  if (out.transport == rocketbox::TransportMode::Sim) {
    if (out.port < 1 || out.port > 4) {
      throw std::runtime_error("sim requires --port N (1–4)");
    }
  } else if (out.port != 0 && (out.port < 1 || out.port > 4)) {
    throw std::runtime_error("--port N must be 1–4 when set");
  }
  if (out.ping_peer != 0 && (out.ping_peer < 1 || out.ping_peer > 4)) {
    throw std::runtime_error("--ping M must be a port 1–4");
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  try {
    if (!parse_args(argc, argv, opt)) {
      return 1;
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    usage(argv[0]);
    return 1;
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  try {
    rocketbox_tunnel_log(std::string("connect transport ") + transport_name(opt.transport) +
                         (opt.port ? " prefer Port " + std::to_string(opt.port) : " (auto Port)"));
    auto transport = open_tunnel_transport(opt.transport, opt.port);
    const int port = transport->display_port();
    if (port < 1 || port > 4) {
      throw std::runtime_error("USB cable has no silkscreen Port (check serial)");
    }
    if (opt.ping_peer != 0 && opt.ping_peer == port) {
      throw std::runtime_error("--ping M must be a different port than local");
    }
    const std::string local_ip = rocketbox_lan::ip_for_port(port);
    if (opt.iface.empty()) opt.iface = "rb" + std::to_string(port);
    rocketbox_tunnel_log("Port " + std::to_string(port) + " address " + local_ip + " system " +
                         transport->system_id() + " serial " + transport->serial());

    TunnelPortLock port_lock;
    std::string lock_err;
    if (!port_lock.try_acquire(port, lock_err)) {
      throw std::runtime_error(lock_err);
    }

    if (opt.ping_peer != 0) {
      return run_rocketbox_ping(*transport, port, opt.ping_peer);
    }

    TunDevice tun;
    tun.open(opt.iface);
    if (opt.use_netns) {
      const std::string ns = "rbns" + std::to_string(port);
      tun.isolate_in_netns(ns, local_ip, port, opt.expose);
      rocketbox_tunnel_log(tun.name() + " " + local_ip + "/24 netns " + ns);
    } else {
      tun.configure_lan(local_ip);
      rocketbox_tunnel_log(tun.name() + " " + local_ip + "/24");
    }

    CircuitDialer dialer(*transport, port);
    TunnelBridge bridge(tun, dialer, port);
    TunnelStatsPublisher stats(port, bridge, transport->serial());
    rocketbox_tunnel_log("bridging (Ctrl+C to stop)");
    std::thread stopper([&] {
      while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      bridge.stop();
      dialer.shutdown();
    });
    bridge.run();
    g_stop = true;
    if (stopper.joinable()) {
      stopper.join();
    }
    transport->disconnect();
    rocketbox_tunnel_log("stopped");
  } catch (const std::exception& e) {
    rocketbox_tunnel_log(std::string("error: ") + e.what());
    return 1;
  }
  return 0;
}
