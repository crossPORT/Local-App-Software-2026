#include "helper/commands_plat.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

namespace tunnel_helper {
namespace {

HANDLE g_child = nullptr;
DWORD g_child_pid = 0;
int g_child_port = 0;

int parse_port_arg(const std::string& cmdline) {
  const auto pos = cmdline.find("--port ");
  if (pos == std::string::npos) return 0;
  try {
    return std::stoi(cmdline.substr(pos + 7));
  } catch (...) {
    return 0;
  }
}

bool child_alive() {
  if (!g_child) return false;
  if (WaitForSingleObject(g_child, 0) != WAIT_OBJECT_0) return true;
  CloseHandle(g_child);
  g_child = nullptr;
  g_child_pid = 0;
  g_child_port = 0;
  return false;
}

}  // namespace

std::string plat_start_child(const std::string& cmdline) {
  if (child_alive()) return "ERR already running\n";
  std::string mutable_cmd = cmdline;
  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  if (!CreateProcessA(nullptr, mutable_cmd.data(), nullptr, nullptr, FALSE,
                      CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    return "ERR CreateProcess failed: " + std::to_string(GetLastError()) + "\n";
  }
  CloseHandle(pi.hThread);
  g_child = pi.hProcess;
  g_child_pid = pi.dwProcessId;
  g_child_port = parse_port_arg(cmdline);
  return "OK started\n";
}

std::string plat_stop_child() {
  if (!child_alive()) return "OK stopped\n";
  GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, g_child_pid);
  if (WaitForSingleObject(g_child, 2000) != WAIT_OBJECT_0) {
    TerminateProcess(g_child, 1);
    WaitForSingleObject(g_child, 5000);
  }
  CloseHandle(g_child);
  g_child = nullptr;
  g_child_pid = 0;
  g_child_port = 0;
  return "OK stopped\n";
}

std::string plat_status_child() {
  if (!child_alive()) return "OK stopped\n";
  std::string out = "OK running pid=" + std::to_string(g_child_pid);
  if (g_child_port > 0) out += " port=" + std::to_string(g_child_port);
  return out + "\n";
}

std::string plat_term_pid(const std::string& arg) {
  try {
    const long pid = std::stol(arg);
    if (pid <= 0) return "ERR bad pid\n";
    HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!h) return "ERR OpenProcess failed\n";
    TerminateProcess(h, 1);
    WaitForSingleObject(h, 5000);
    CloseHandle(h);
    if (g_child_pid == static_cast<DWORD>(pid)) {
      if (g_child) CloseHandle(g_child);
      g_child = nullptr;
      g_child_pid = 0;
      g_child_port = 0;
    }
    return "OK\n";
  } catch (...) {
    return "ERR bad pid\n";
  }
}

}  // namespace tunnel_helper
