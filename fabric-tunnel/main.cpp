#include "bridge.hpp"
#include "dialer.hpp"
#include "fabric_ping.hpp"
#include "peer_map.hpp"
#include "tun_device.hpp"

#include "rocketbox/sdk.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <sstream>
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
  bool use_netns = true;
  rocketbox::TransportType transport = rocketbox::TransportType::Usb;
  std::vector<int> expose_ports;  // empty = publish nothing (secure default)
};

const char* transport_name(rocketbox::TransportType t) {
  return t == rocketbox::TransportType::Usb ? "usb" : "sim";
}

void usage(const char* argv0) {
  std::cerr
      << "RocketBox fabric–TCP bridge\n"
      << "Usage: " << argv0 << " --port N [options]\n"
      << "  --port N           Fabric port 1–4 (assigns 10.64.0.N)\n"
      << "  --transport T      usb (default) or sim\n"
      << "  --expose P[,P...]  Publish these host TCP/UDP ports on 10.64.0.N (default: none)\n"
      << "  --iface NAME       TUN interface name (default: rbN)\n"
      << "  --no-netns         Keep TUN in the host network namespace\n"
      << "  --ping M           ICMP echo to fabric peer port M, then exit\n";
}

void parse_expose(const std::string& s, std::vector<int>& out) {
  std::stringstream ss(s);
  std::string part;
  while (std::getline(ss, part, ',')) {
    if (part.empty()) {
      continue;
    }
    const int p = std::stoi(part);
    if (p <= 0 || p > 65535) {
      throw std::runtime_error("invalid --expose port: " + part);
    }
    out.push_back(p);
  }
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
        out.transport = rocketbox::TransportType::Usb;
      } else if (t == "sim") {
        out.transport = rocketbox::TransportType::Sim;
      } else {
        throw std::runtime_error("--transport must be usb or sim");
      }
    } else if (a == "--ping") {
      out.ping_peer = std::stoi(need("--ping"));
    } else if (a == "--expose") {
      parse_expose(need("--expose"), out.expose_ports);
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
  if (out.port < 1 || out.port > 4) {
    usage(argv[0]);
    return false;
  }
  if (out.ping_peer != 0 && (out.ping_peer < 1 || out.ping_peer > 4 || out.ping_peer == out.port)) {
    throw std::runtime_error("--ping M must be a different fabric port 1..4");
  }
  if (out.iface.empty()) {
    out.iface = "rb" + std::to_string(out.port);
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
    const std::string local_ip = fabric_lan::ip_for_port(opt.port);
    std::cerr << "[rocketbox-tunnel] attach port " << opt.port << " address " << local_ip
              << " transport " << transport_name(opt.transport) << std::endl;
    auto session = rocketbox::Attach(opt.transport, opt.port);
    std::cerr << "[rocketbox-tunnel] system " << session->GetSystemId() << std::endl;

    if (opt.ping_peer != 0) {
      return run_fabric_ping(*session, opt.port, opt.ping_peer);
    }

    TunDevice tun;
    tun.open(opt.iface);
    if (opt.use_netns) {
      const std::string ns = "rbns" + std::to_string(opt.port);
      tun.isolate_in_netns(ns, local_ip, opt.port, opt.expose_ports);
      std::cerr << "[rocketbox-tunnel] " << tun.name() << " " << local_ip << "/24 netns " << ns;
      if (!opt.expose_ports.empty()) {
        std::cerr << " expose";
        for (int p : opt.expose_ports) {
          std::cerr << " :" << p;
        }
      }
      std::cerr << std::endl;
    } else {
      tun.configure_lan(local_ip);
      std::cerr << "[rocketbox-tunnel] " << tun.name() << " " << local_ip << "/24" << std::endl;
    }

    CircuitDialer dialer(*session, opt.port);
    TunnelBridge bridge(tun, dialer, opt.port);
    std::thread stopper([&] {
      while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        dialer.tick_idle();
      }
      bridge.stop();
      dialer.shutdown();
      tun.interrupt();
    });
    bridge.run();
    g_stop = true;
    if (stopper.joinable()) {
      stopper.join();
    }
  } catch (const std::exception& e) {
    std::cerr << "[rocketbox-tunnel] error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
