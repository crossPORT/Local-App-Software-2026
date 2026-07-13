#include "helper_launch_win.hpp"

#include <chrono>
#include <string>
#include <thread>

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

std::string self_dir() {
  char self[MAX_PATH];
  const DWORD n = GetModuleFileNameA(nullptr, self, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return {};
  return dirname_of(self);
}

}  // namespace

std::string default_helper_bin() {
  const std::string dir = self_dir();
  if (!dir.empty()) {
    const std::string cand = dir + "\\rocketbox-tunnel-helper.exe";
    if (file_exists(cand)) return cand;
  }
  return "rocketbox-tunnel-helper.exe";
}

bool helper_responds() {
  std::string reply, err;
  return tunnel_helper::send_command("STATUS", reply, err);
}

bool ensure_helper_elevated(std::string& error) {
  if (helper_responds()) return true;
  const std::wstring whelp = utf8_to_wide(default_helper_bin());
  SHELLEXECUTEINFOW sei{};
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_NOCLOSEPROCESS;
  sei.lpVerb = L"runas";
  sei.lpFile = whelp.c_str();
  sei.nShow = SW_HIDE;
  if (!ShellExecuteExW(&sei)) {
    error = "UAC cancelled — approve elevation for RocketBox Tunnel helper";
    return false;
  }
  if (sei.hProcess) CloseHandle(sei.hProcess);
  for (int i = 0; i < 300; ++i) {
    if (helper_responds()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  error = "tunnel helper did not start (approve UAC once, keep helper running)";
  return false;
}

}  // namespace tunnel_tray
