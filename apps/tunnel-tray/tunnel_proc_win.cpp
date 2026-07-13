#include "tunnel_proc.hpp"
#include "platform/stats_paths.hpp"
#include "tunnel_args.hpp"

#include <chrono>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

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

std::wstring utf8_to_wide(const std::string& s) {
  if (s.empty()) return L"";
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
  return out;
}

std::string build_params(const TunnelConfig& cfg, const std::string& bin) {
  // Skip argv0 — ShellExecute takes file + parameters separately.
  const std::string tail = build_tunnel_arg_tail(cfg, bin);
  const auto sp = tail.find(' ');
  if (sp == std::string::npos) return {};
  return tail.substr(sp + 1);
}

}  // namespace

std::string default_tunnel_bin() {
  if (const char* env = std::getenv("ROCKETBOX_TUNNEL_PATH")) {
    if (file_exists(env)) return env;
  }
  char self[MAX_PATH];
  const DWORD n = GetModuleFileNameA(nullptr, self, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    const std::string dir = dirname_of(self);
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

bool TunnelProcess::tunnel_ready(const TunnelConfig& cfg) {
  if (cfg.port >= 1 && cfg.port <= 4 && file_exists(tunnel_stats_path(cfg.port))) return true;
  if (cfg.port == 0) {
    for (int p = 1; p <= 4; ++p) {
      if (file_exists(tunnel_stats_path(p))) return true;
    }
  }
  return false;
}

bool TunnelProcess::running() const {
  if (helper_managed_) return file_exists(tunnel_stats_path(port_));
  if (pid_ <= 0) return false;
  return !child_exited(nullptr);
}

bool TunnelProcess::start(const TunnelConfig& cfg, std::string& error) {
  if (running()) {
    error = "tunnel already running";
    return false;
  }
  const std::string bin = cfg.tunnel_bin.empty() ? default_tunnel_bin() : cfg.tunnel_bin;
  TunnelConfig run = cfg;
  run.use_netns = false;
  run.expose.clear();  // Windows --expose not enabled yet
  const std::string tail = build_tunnel_arg_tail(run, bin);

  std::string reply, herr;
  if (tunnel_helper::send_command("START " + tail, reply, herr) && reply.rfind("OK", 0) == 0) {
    helper_managed_ = true;
    port_ = cfg.port;
    for (int i = 0; i < 600; ++i) {
      if (tunnel_ready(cfg)) return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    (void)tunnel_helper::send_command("STOP", reply, herr);
    helper_managed_ = false;
    error = "helper started tunnel but it did not become ready";
    return false;
  }

  const std::wstring wbin = utf8_to_wide(bin);
  const std::wstring wparams = utf8_to_wide(build_params(run, bin));
  SHELLEXECUTEINFOW sei{};
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_NOCLOSEPROCESS;
  sei.lpVerb = L"runas";
  sei.lpFile = wbin.c_str();
  sei.lpParameters = wparams.empty() ? nullptr : wparams.c_str();
  sei.nShow = SW_HIDE;
  if (!ShellExecuteExW(&sei) || !sei.hProcess) {
    error = "authorization cancelled or failed to start tunnel";
    return false;
  }
  pid_ = static_cast<long>(GetProcessId(sei.hProcess));
  CloseHandle(sei.hProcess);
  port_ = cfg.port;
  helper_managed_ = false;

  for (int i = 0; i < 600; ++i) {
    if (child_exited(nullptr)) {
      error = "authorization cancelled or tunnel failed to start";
      return false;
    }
    if (tunnel_ready(cfg)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  error = "tunnel failed to become ready";
  stop();
  return false;
}

void TunnelProcess::stop() {
  if (helper_managed_) {
    std::string reply, herr;
    (void)tunnel_helper::send_command("STOP", reply, herr);
    helper_managed_ = false;
    pid_ = 0;
    return;
  }
  if (pid_ <= 0) return;
  HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid_));
  if (h) {
    TerminateProcess(h, 1);
    WaitForSingleObject(h, 5000);
    CloseHandle(h);
  }
  pid_ = 0;
}

}  // namespace tunnel_tray
