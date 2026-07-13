#include "tunnel_stats.hpp"
#include "stats_path.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

long self_pid() {
#if defined(_WIN32)
  return static_cast<long>(GetCurrentProcessId());
#else
  return static_cast<long>(::getpid());
#endif
}

}  // namespace

TunnelStatsPublisher::TunnelStatsPublisher(int port, TunnelBridge& bridge, std::string serial)
    : port_(port), serial_(std::move(serial)), bridge_(bridge) {
  std::error_code ec;
  const auto dir = rocketbox_tunnel_stats_dir();
  std::filesystem::create_directories(dir, ec);
#if !defined(_WIN32)
  ::chmod(dir.c_str(), 0755);
#endif
  write_file(0, 0, 0, 0);  // ready signal as soon as bridging starts
  thr_ = std::thread([this] { loop(); });
}


TunnelStatsPublisher::~TunnelStatsPublisher() {
  stop_ = true;
  if (thr_.joinable()) thr_.join();
  unlink_file();
}

void TunnelStatsPublisher::loop() {
  uint64_t prev_up = bridge_.upstream_bytes();
  uint64_t prev_down = bridge_.downstream_bytes();
  while (!stop_) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (stop_) break;
    const uint64_t up = bridge_.upstream_bytes();
    const uint64_t down = bridge_.downstream_bytes();
    write_file(up - prev_up, down - prev_down, up, down);
    prev_up = up;
    prev_down = down;
  }
}

void TunnelStatsPublisher::write_file(uint64_t up_bps, uint64_t down_bps, uint64_t up_tot,
                                      uint64_t down_tot) {
  const std::string path = rocketbox_tunnel_stats_path(port_);
  const std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp);
    if (!out) return;
    out << "up_bps=" << up_bps << "\n"
        << "down_bps=" << down_bps << "\n"
        << "up_bytes=" << up_tot << "\n"
        << "down_bytes=" << down_tot << "\n"
        << "display_port=" << port_ << "\n"
        << "serial=" << serial_ << "\n"
        << "pid=" << self_pid() << "\n";
  }
#if !defined(_WIN32)
  ::chmod(tmp.c_str(), 0644);
#endif
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
}

void TunnelStatsPublisher::unlink_file() {
  std::error_code ec;
  std::filesystem::remove(rocketbox_tunnel_stats_path(port_), ec);
}
