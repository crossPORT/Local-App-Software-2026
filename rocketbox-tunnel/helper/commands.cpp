#include "helper/commands.hpp"

#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tunnel_helper {
namespace {

#if defined(_WIN32)
HANDLE g_child = nullptr;
DWORD g_child_pid = 0;

bool child_alive() {
  if (!g_child) return false;
  const DWORD w = WaitForSingleObject(g_child, 0);
  if (w == WAIT_OBJECT_0) {
    CloseHandle(g_child);
    g_child = nullptr;
    g_child_pid = 0;
    return false;
  }
  return true;
}

std::string start_child(const std::string& cmdline) {
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
  return "OK started\n";
}

std::string stop_child() {
  if (!child_alive()) return "OK stopped\n";
  TerminateProcess(g_child, 1);
  WaitForSingleObject(g_child, 5000);
  CloseHandle(g_child);
  g_child = nullptr;
  g_child_pid = 0;
  return "OK stopped\n";
}

std::string status_child() {
  return child_alive() ? "OK running\n" : "OK stopped\n";
}
#else
int g_child = 0;

bool child_alive() {
  if (g_child <= 0) return false;
  if (::kill(g_child, 0) == 0) return true;
  (void)::waitpid(g_child, nullptr, WNOHANG);
  g_child = 0;
  return false;
}

std::string start_child(const std::string& cmdline) {
  if (child_alive()) return "ERR already running\n";
  const pid_t pid = ::fork();
  if (pid == 0) {
    ::setsid();
    ::execl("/bin/sh", "sh", "-c", cmdline.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
  if (pid < 0) return "ERR fork failed\n";
  g_child = static_cast<int>(pid);
  return "OK started\n";
}

std::string stop_child() {
  if (g_child > 0) {
    ::kill(g_child, SIGTERM);
    ::waitpid(g_child, nullptr, 0);
    g_child = 0;
  }
  return "OK stopped\n";
}

std::string status_child() {
  if (!child_alive()) return "OK stopped\n";
  return "OK running\n";
}
#endif

}  // namespace

std::string handle_line(const std::string& line) {
  if (line.rfind("START ", 0) == 0) return start_child(line.substr(6));
  if (line == "STOP") return stop_child();
  if (line == "STATUS") return status_child();
  return "ERR unknown\n";
}

void shutdown_child() { (void)stop_child(); }

}  // namespace tunnel_helper
