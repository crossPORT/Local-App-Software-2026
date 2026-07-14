#include "tunnel_proc.hpp"
#include "helper_launch_win.hpp"
#include "platform/stats_paths.hpp"
#include "tunnel_args.hpp"
#include "tunnel_hup.hpp"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "helper/protocol.hpp"

namespace tunnel_tray {
namespace {

bool file_exists(const std::string& path) {
  const DWORD a = GetFileAttributesA(path.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::string dirname_of(const std::string& path) {
  const auto pos = path.find_last_of("\\/");
  if (pos == std::string::npos) return ".";
  if (pos == 0) return path.substr(0, 1);
  return path.substr(0, pos);
}

std::string self_dir() {
  char self[MAX_PATH];
  const DWORD n = GetModuleFileNameA(nullptr, self, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return {};
  return dirname_of(self);
}

}  // namespace

std::string default_tunnel_bin() {
  if (const char* env = std::getenv("ROCKETBOX_TUNNEL_PATH")) {
    if (file_exists(env)) return env;
  }
  const std::string dir = self_dir();
  if (!dir.empty()) {
    const std::string cand = dir + "\\rocketbox-tunnel.exe";
    if (file_exists(cand)) return cand;
  }
  return "rocketbox-tunnel.exe";
}

bool TunnelProcess::child_exited(int* status_out) const {
  if (pid_ <= 0) return true;
  HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                         static_cast<DWORD>(pid_));
  if (!h) {
    pid_ = 0;
    return true;
  }
  const DWORD w = WaitForSingleObject(h, 0);
  if (w == WAIT_OBJECT_0) {
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    pid_ = 0;
    if (status_out) *status_out = static_cast<int>(code);
    return true;
  }
  CloseHandle(h);
  return false;
}

bool TunnelProcess::tunnel_ready(const TunnelConfig& cfg) const {
  int ready_port = 0;
  return stats_ready_new_pid(cfg.port, pids_at_start_, &ready_port);
}

bool TunnelProcess::running() const {
  std::string st, herr;
  if (tunnel_helper::send_command("STATUS", st, herr) && st.rfind("OK running", 0) == 0) {
    helper_managed_ = true;
    return true;
  }
  if (helper_managed_) helper_managed_ = false;
  if (live_tunnel_port() > 0) return true;
  if (pid_ <= 0) return false;
  return !child_exited(nullptr);
}

bool TunnelProcess::start(const TunnelConfig& cfg, std::string& error) {
  if (running()) {
    const int live = live_tunnel_port();
    TunnelConfig adopted = cfg;
    if (live > 0) {
      adopted.port = live;
      port_ = live;
    }
    std::string ignore;
    (void)reload(adopted, ignore);
    return true;
  }
  const std::string bin = cfg.tunnel_bin.empty() ? default_tunnel_bin() : cfg.tunnel_bin;
  TunnelConfig run = cfg;
  run.use_netns = false;
  const std::string tail = build_tunnel_arg_tail(run, bin);

  if (!ensure_helper_elevated(error)) return false;

  pids_at_start_ = snapshot_stats_pids();
  clear_tunnel_stats(cfg.port);

  std::string reply, herr;
  if (!tunnel_helper::send_command("START " + tail, reply, herr) || reply.rfind("OK", 0) != 0) {
    error = herr.empty() ? (reply.empty() ? "helper START failed" : reply) : herr;
    while (!error.empty() && (error.back() == '\n' || error.back() == '\r')) error.pop_back();
    if (error.find("already running") != std::string::npos) {
      helper_managed_ = true;
      const int live = live_tunnel_port();
      if (live > 0) port_ = live;
      else if (cfg.port >= 1 && cfg.port <= 4) port_ = cfg.port;
      return true;
    }
    if (error.rfind("ERR ", 0) == 0) error.erase(0, 4);
    return false;
  }
  helper_managed_ = true;
  port_ = cfg.port;
  for (int i = 0; i < 600; ++i) {
    int ready = 0;
    if (stats_ready_new_pid(cfg.port, pids_at_start_, &ready)) {
      port_ = ready;
      return true;
    }
    std::string st;
    if (tunnel_helper::send_command("STATUS", st, herr) && st.rfind("OK stopped", 0) == 0) {
      helper_managed_ = false;
      error = "tunnel exited (need Admin/Wintun; check log)";
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  (void)tunnel_helper::send_command("STOP", reply, herr);
  helper_managed_ = false;
  error = "helper started tunnel but it did not become ready";
  return false;
}

bool TunnelProcess::reload(const TunnelConfig& cfg, std::string& error) {
  int port = cfg.port;
  if (!live_tunnel_holds_port(port)) {
    const int live = live_tunnel_port();
    if (live > 0) port = live;
  }
  if (port < 1 || port > 4) {
    error = "no Port to reload expose";
    return false;
  }
  write_expose_from_endpoints(port, cfg.expose);
  port_ = port;
  return true;
}

}  // namespace tunnel_tray
