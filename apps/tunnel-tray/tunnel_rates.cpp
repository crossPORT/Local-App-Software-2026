#include "tunnel_proc.hpp"
#include "platform/stats_paths.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace tunnel_tray {

TunnelRates read_tunnel_rates(int port) {
  TunnelRates r;
  std::ifstream in(tunnel_stats_path(port));
  if (!in) return r;
  std::string line;
  while (std::getline(in, line)) {
    if (line.compare(0, 7, "up_bps=") == 0) {
      r.up_bps = std::stoull(line.substr(7));
    } else if (line.compare(0, 9, "down_bps=") == 0) {
      r.down_bps = std::stoull(line.substr(9));
    } else if (line.compare(0, 13, "display_port=") == 0) {
      r.display_port = std::stoi(line.substr(13));
    } else if (line.compare(0, 4, "pid=") == 0) {
      r.pid = std::stol(line.substr(4));
    } else if (line.compare(0, 7, "serial=") == 0) {
      r.serial = line.substr(7);
    }
  }
  r.ok = true;
  return r;
}

std::vector<long> snapshot_stats_pids() {
  std::vector<long> out;
  for (int p = 1; p <= 4; ++p) {
    const auto r = read_tunnel_rates(p);
    if (r.ok && r.pid > 0) out.push_back(r.pid);
  }
  return out;
}

bool stats_ready_new_pid(int prefer_port, const std::vector<long>& before, int* port_out) {
  auto is_new = [&](long pid) {
    return pid > 0 && std::find(before.begin(), before.end(), pid) == before.end();
  };
  auto check = [&](int p) {
    if (p < 1 || p > 4) return false;
    const auto r = read_tunnel_rates(p);
    if (!r.ok || !is_new(r.pid)) return false;
    if (port_out) *port_out = p;
    return true;
  };
  if (check(prefer_port)) return true;
  if (prefer_port != 0) return false;
  for (int p = 1; p <= 4; ++p)
    if (check(p)) return true;
  return false;
}

std::string format_rate(uint64_t bps) {
  if (bps >= 1000 * 1000) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f MB/s", bps / (1000.0 * 1000.0));
    return buf;
  }
  if (bps >= 1000) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f KB/s", bps / 1000.0);
    return buf;
  }
  return std::to_string(bps) + " B/s";
}

bool live_tunnel_holds_port(int display_port) {
  if (display_port < 1 || display_port > 4) return false;
  const auto r = read_tunnel_rates(display_port);
  if (!r.ok || r.pid <= 0) return false;
#if defined(_WIN32)
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(r.pid));
  if (!h) return false;
  DWORD code = 0;
  const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
  CloseHandle(h);
  return alive;
#else
  if (::kill(static_cast<pid_t>(r.pid), 0) == 0) return true;
  return errno == EPERM;
#endif
}

}  // namespace tunnel_tray
