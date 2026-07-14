#pragma once

#include "host_gateway.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/** Linux TUN (IFF_TUN | IFF_NO_PI) for RocketBox fabric LAN. */
class TunDevice {
public:
  TunDevice() = default;
  ~TunDevice();

  TunDevice(const TunDevice&) = delete;
  TunDevice& operator=(const TunDevice&) = delete;

  void open(const std::string& iface_name);
  void configure_lan(const std::string& local_ip);
  /** Netns + host gateway. expose = allowlist published on fabric IP. */
  void isolate_in_netns(const std::string& netns, const std::string& local_ip, int local_port,
                        const std::vector<ExposeRule>& expose);
  /** No-netns path: install host expose filter (Windows/macOS; Linux --no-netns). */
  void install_expose_filter(int local_port, const std::vector<ExposeRule>& expose);
  /** Reload published ports (SIGHUP / expose file change). */
  void reload_expose(const std::vector<ExposeRule>& expose);
  void close();
  void interrupt();

  int fd() const { return fd_; }
  const std::string& name() const { return name_; }
  const std::string& netns() const { return netns_; }

  std::vector<uint8_t> read_packet();
  void write_packet(const uint8_t* data, size_t len);

private:
  int fd_ = -1;
  void* win_ = nullptr;  // WinTunCtx* on Windows
  std::string name_;
  std::string netns_;
  std::unique_ptr<HostGateway> gateway_;
};
