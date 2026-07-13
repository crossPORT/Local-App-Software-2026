#include "open_transport.hpp"

#include "rocketbox/port_probe.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

int resolve_usb_display_port(int requested) {
  const auto ports = rocketbox::list_present_ports();
  std::vector<rocketbox::PresentPort> avail;
  for (const auto& p : ports) {
    if (p.display_port < 1 || p.display_port > 4 || !p.available) continue;
    if (requested != 0 && p.display_port != requested) continue;
    avail.push_back(p);
  }
  if (requested == 0) {
    if (avail.empty()) {
      throw std::runtime_error("no available RocketBox USB cable");
    }
    if (avail.size() > 1) {
      throw std::runtime_error(
          "multiple USB cables — pass --port N to pick (silkscreen Port from serial)");
    }
    return avail.front().display_port;
  }
  if (avail.empty()) {
    bool saw = false;
    for (const auto& p : ports) {
      if (p.display_port == requested) {
        saw = true;
        if (!p.available) {
          throw std::runtime_error(
              "USB Port " + std::to_string(requested) +
              " in use — close RocketBox App on that cable and retry");
        }
      }
    }
    throw std::runtime_error(saw ? "USB Port " + std::to_string(requested) + " unavailable"
                                 : "no RocketBox USB cable for Port " +
                                       std::to_string(requested));
  }
  return requested;
}

}  // namespace

std::unique_ptr<rocketbox::RocketBoxTransport> open_tunnel_transport(
    rocketbox::TransportMode mode, int display_port) {
  if (mode == rocketbox::TransportMode::Sim) {
    if (display_port < 1 || display_port > 4) {
      throw std::runtime_error("sim requires --port N (1–4)");
    }
    auto t = rocketbox::create_rocketbox_transport(mode, display_port);
    t->connect();
    return t;
  }

  const int want = resolve_usb_display_port(display_port);
  const int libusb_index = rocketbox::libusb_index_for_display_port(want);
  if (libusb_index < 0) {
    throw std::runtime_error("no RocketBox USB cable for Port " + std::to_string(want));
  }

  auto t = rocketbox::create_rocketbox_transport(rocketbox::TransportMode::Usb,
                                                libusb_index + 1);
  t->connect();
  if (t->display_port() != want) {
    throw std::runtime_error("USB serial maps to Port " +
                             std::to_string(t->display_port()) +
                             ", expected Port " + std::to_string(want));
  }
  return t;
}
