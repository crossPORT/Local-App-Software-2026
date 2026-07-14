#include "bridge.hpp"
#include "dialer.hpp"
#include "expose_file.hpp"
#include "open_transport.hpp"
#include "peer_map.hpp"
#include "port_lock.hpp"
#include "rocketbox_ping.hpp"
#include "tun_device.hpp"
#include "tunnel_log.hpp"
#include "tunnel_options.hpp"
#include "tunnel_stats.hpp"

#include "rocketbox/sdk.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

std::atomic<bool> g_stop{false};
std::atomic<bool> g_reload{false};

void on_signal(int sig) {
#if defined(SIGHUP)
  if (sig == SIGHUP) {
    g_reload = true;
    return;
  }
#endif
  g_stop = true;
}

}  // namespace

int main(int argc, char** argv) {
  TunnelOptions opt;
  try {
    if (!tunnel_parse_args(argc, argv, opt)) {
      return 1;
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    tunnel_usage(argv[0]);
    return 1;
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);
#if defined(SIGHUP)
  std::signal(SIGHUP, on_signal);
#endif

  try {
    rocketbox_tunnel_log(std::string("connect transport ") + tunnel_transport_name(opt.transport) +
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

    write_expose_file(port, opt.expose);

    TunDevice tun;
    tun.open(opt.iface);
    if (opt.use_netns) {
      const std::string ns = "rbns" + std::to_string(port);
      tun.isolate_in_netns(ns, local_ip, port, opt.expose);
      rocketbox_tunnel_log(tun.name() + " " + local_ip + "/24 netns " + ns);
    } else {
      tun.configure_lan(local_ip);
      tun.install_expose_filter(port, opt.expose);
      rocketbox_tunnel_log(tun.name() + " " + local_ip + "/24 expose " +
                           std::to_string(opt.expose.size()) + " rules");
    }

    CircuitDialer dialer(*transport, port);
    TunnelBridge bridge(tun, dialer, port);
    TunnelStatsPublisher stats(port, bridge, transport->serial());
    rocketbox_tunnel_log("bridging (Ctrl+C to stop; reload expose via SIGHUP or expose file)");
    std::thread stopper([&] {
      std::error_code ec0;
      auto last_expose = std::filesystem::last_write_time(rocketbox_expose_path(port), ec0);
      bool have_mtime = !ec0;
      while (!g_stop) {
        bool do_reload = g_reload.exchange(false);
        std::error_code ec;
        const auto mtime = std::filesystem::last_write_time(rocketbox_expose_path(port), ec);
        if (!ec && have_mtime && mtime != last_expose) {
          last_expose = mtime;
          do_reload = true;
        } else if (!ec && !have_mtime) {
          last_expose = mtime;
          have_mtime = true;
        }
        if (do_reload) {
          try {
            const auto rules = read_expose_file(port);
            tun.reload_expose(rules);
            rocketbox_tunnel_log("reloaded expose (" + std::to_string(rules.size()) + " rules)");
          } catch (const std::exception& e) {
            rocketbox_tunnel_log(std::string("reload failed: ") + e.what());
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      bridge.stop();
      dialer.shutdown();
    });
    bridge.run();
    g_stop = true;
    if (stopper.joinable()) stopper.join();
    transport->disconnect();
    rocketbox_tunnel_log("stopped");
  } catch (const std::exception& e) {
    rocketbox_tunnel_log(std::string("error: ") + e.what());
    return 1;
  }
  return 0;
}
