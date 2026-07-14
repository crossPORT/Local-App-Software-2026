#include "helper/commands.hpp"

#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <csignal>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tunnel_helper {
namespace {

int parse_port_arg(const std::string& cmdline) {
  const auto pos = cmdline.find("--port ");
  if (pos == std::string::npos) return 0;
  try {
    return std::stoi(cmdline.substr(pos + 7));
  } catch (...) {
    return 0;
  }
}

#if defined(_WIN32)
HANDLE g_child = nullptr;
DWORD g_child_pid = 0;
int g_child_port = 0;

bool child_alive() {
  if (!g_child) return false;
  if (WaitForSingleObject(g_child, 0) != WAIT_OBJECT_0) return true;
  CloseHandle(g_child);
  g_child = nullptr;
  g_child_pid = 0;
  g_child_port = 0;
  return false;
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
  g_child_port = parse_port_arg(cmdline);
  return "OK started\n";
}

std::string stop_child() {
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

std::string status_child() {
  if (!child_alive()) return "OK stopped\n";
  std::string out = "OK running pid=" + std::to_string(g_child_pid);
  if (g_child_port > 0) out += " port=" + std::to_string(g_child_port);
  return out + "\n";
}

std::string term_pid(const std::string& arg) {
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
#else
int g_child = 0;
int g_child_port = 0;

bool child_alive() {
  if (g_child <= 0) return false;
  if (::kill(g_child, 0) == 0) return true;
  (void)::waitpid(g_child, nullptr, WNOHANG);
  g_child = 0;
  g_child_port = 0;
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
  g_child_port = parse_port_arg(cmdline);
  return "OK started\n";
}

std::string stop_child() {
  if (g_child > 0) {
    ::kill(g_child, SIGTERM);
    ::waitpid(g_child, nullptr, 0);
    g_child = 0;
    g_child_port = 0;
  }
  return "OK stopped\n";
}

std::string status_child() {
  if (!child_alive()) return "OK stopped\n";
  std::string out = "OK running pid=" + std::to_string(g_child);
  if (g_child_port > 0) out += " port=" + std::to_string(g_child_port);
  return out + "\n";
}

std::string hup_managed() {
  if (!child_alive()) return "ERR not running\n";
  if (::kill(g_child, SIGHUP) != 0)
    return std::string("ERR HUP failed: ") + std::strerror(errno) + "\n";
  return "OK\n";
}

std::string signal_pid(const std::string& arg, int sig) {
  try {
    const long pid = std::stol(arg);
    if (pid <= 0) return "ERR bad pid\n";
    if (::kill(static_cast<pid_t>(pid), sig) != 0)
      return std::string("ERR signal failed: ") + std::strerror(errno) + "\n";
    return "OK\n";
  } catch (...) {
    return "ERR bad pid\n";
  }
}
#endif

}  // namespace

std::string handle_line(const std::string& line) {
  if (line.rfind("START ", 0) == 0) return start_child(line.substr(6));
  if (line == "STOP") return stop_child();
  if (line == "STATUS") return status_child();
#if defined(_WIN32)
  if (line.rfind("TERM ", 0) == 0) return term_pid(line.substr(5));
#else
  if (line == "HUP") return hup_managed();
  if (line.rfind("HUP ", 0) == 0) return signal_pid(line.substr(4), SIGHUP);
  if (line.rfind("TERM ", 0) == 0) return signal_pid(line.substr(5), SIGTERM);
#endif
  return "ERR unknown\n";
}

void shutdown_child() { (void)stop_child(); }

}  // namespace tunnel_helper
