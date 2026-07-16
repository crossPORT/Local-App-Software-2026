#include "helper/commands_plat.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace tunnel_helper {
namespace {

int g_child = 0;
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

std::vector<std::string> split_cmdline(const std::string& cmdline) {
  std::vector<std::string> args;
  std::string cur;
  bool in_quote = false;
  for (char c : cmdline) {
    if (c == '"') {
      in_quote = !in_quote;
      continue;
    }
    if (!in_quote && (c == ' ' || c == '\t')) {
      if (!cur.empty()) {
        args.push_back(cur);
        cur.clear();
      }
      continue;
    }
    cur.push_back(c);
  }
  if (!cur.empty()) args.push_back(cur);
  return args;
}

bool child_alive() {
  if (g_child <= 0) return false;
  if (::kill(g_child, 0) == 0) return true;
  (void)::waitpid(g_child, nullptr, WNOHANG);
  g_child = 0;
  g_child_port = 0;
  return false;
}

bool wait_reap(pid_t pid, int timeout_tenths) {
  for (int i = 0; i < timeout_tenths; ++i) {
    int st = 0;
    const pid_t r = ::waitpid(pid, &st, WNOHANG);
    if (r == pid || (r < 0 && errno == ECHILD)) return true;
    ::usleep(100000);
  }
  return false;
}

}  // namespace

std::string plat_start_child(const std::string& cmdline) {
  if (child_alive()) return "ERR already running\n";
  auto args = split_cmdline(cmdline);
  if (args.empty()) return "ERR empty cmdline\n";
  const pid_t pid = ::fork();
  if (pid == 0) {
    ::setsid();
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);
    ::execv(argv[0], argv.data());
    _exit(127);
  }
  if (pid < 0) return "ERR fork failed\n";
  g_child = static_cast<int>(pid);
  g_child_port = parse_port_arg(cmdline);
  return "OK started\n";
}

std::string plat_stop_child() {
  if (g_child <= 0) return "OK stopped\n";
  const pid_t pid = static_cast<pid_t>(g_child);
  // g_child is rocketbox-tunnel itself (no shell). SIGTERM → g_stop; escalate if stuck.
  (void)::kill(pid, SIGTERM);
  if (!wait_reap(pid, 30)) {
    (void)::kill(pid, SIGKILL);
    (void)::waitpid(pid, nullptr, 0);
  }
  g_child = 0;
  g_child_port = 0;
  return "OK stopped\n";
}

std::string plat_status_child() {
  if (!child_alive()) return "OK stopped\n";
  std::string out = "OK running pid=" + std::to_string(g_child);
  if (g_child_port > 0) out += " port=" + std::to_string(g_child_port);
  return out + "\n";
}

std::string plat_hup_managed() {
  if (!child_alive()) return "ERR not running\n";
  if (::kill(g_child, SIGHUP) != 0)
    return std::string("ERR HUP failed: ") + std::strerror(errno) + "\n";
  return "OK\n";
}

std::string plat_signal_pid(const std::string& arg, int sig) {
  try {
    const long pid = std::stol(arg);
    if (pid <= 0) return "ERR bad pid\n";
    if (::kill(static_cast<pid_t>(pid), sig) != 0)
      return std::string("ERR signal failed: ") + std::strerror(errno) + "\n";
    if (sig == SIGTERM) {
      for (int i = 0; i < 30; ++i) {
        if (::kill(static_cast<pid_t>(pid), 0) != 0) return "OK\n";
        ::usleep(100000);
      }
      (void)::kill(static_cast<pid_t>(pid), SIGKILL);
    }
    return "OK\n";
  } catch (...) {
    return "ERR bad pid\n";
  }
}

}  // namespace tunnel_helper
