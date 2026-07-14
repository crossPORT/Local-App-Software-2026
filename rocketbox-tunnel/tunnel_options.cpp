#include "tunnel_options.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

const char* tunnel_transport_name(rocketbox::TransportMode t) {
  return t == rocketbox::TransportMode::Usb ? "usb" : "sim";
}

void tunnel_usage(const char* argv0) {
  std::cerr << "RocketBox tunnel (USB ↔ host IP)\n"
            << "Usage: " << argv0 << " [options]\n"
            << "  (USB default)      Port from cable serial → 10.64.0.N\n"
            << "  --port N           USB: pick among multiple cables; sim: required\n"
            << "  --transport T      usb (default) or sim\n"
            << "  --expose SPEC      Publish host ports: 445, tcp:22, udp:53\n"
            << "  --iface NAME       TUN interface name (default: rbN)\n"
            << "  --no-netns         Keep TUN in the host network namespace\n"
            << "  --ping M           ICMP echo to peer port M, then exit\n"
            << "  SIGHUP             Reload expose from /tmp/rocketbox/tunnel-N.expose\n";
}

bool tunnel_parse_args(int argc, char** argv, TunnelOptions& out) {
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
      tunnel_usage(argv[0]);
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
