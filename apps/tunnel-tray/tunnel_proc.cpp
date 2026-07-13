#include "tunnel_proc.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace tunnel_tray {
namespace {

bool file_executable(const std::string& path) {
  struct stat st {};
  return ::stat(path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR);
}

std::string dirname_of(const std::string& path) {
  const auto pos = path.find_last_of('/');
  if (pos == std::string::npos) return ".";
  if (pos == 0) return "/";
  return path.substr(0, pos);
}

}  // namespace

std::string default_tunnel_bin() {
  if (const char* env = std::getenv("ROCKETBOX_TUNNEL_PATH")) {
    if (file_executable(env)) return env;
  }
  char self[4096];
  const ssize_t n = ::readlink("/proc/self/exe", self, sizeof(self) - 1);
  if (n > 0) {
    self[n] = '\0';
    const std::string dir = dirname_of(self);
    const std::string candidates[] = {
        dir + "/rocketbox-tunnel",
        dirname_of(dir) + "/rocketbox-tunnel/rocketbox-tunnel",
        dirname_of(dirname_of(dir)) + "/rocketbox-tunnel/rocketbox-tunnel",
    };
    for (const auto& path : candidates) {
      if (file_executable(path)) return path;
    }
  }
  return "rocketbox-tunnel";
}

bool TunnelProcess::running() const {
  if (pid_ <= 0) return false;
  if (::kill(static_cast<pid_t>(pid_), 0) == 0) return true;
  return errno == EPERM;  // exists but not signalable (e.g. root via pkexec)
}

bool TunnelProcess::start(const TunnelConfig& cfg, std::string& error) {
  if (running()) {
    error = "tunnel already running";
    return false;
  }

  const std::string bin = cfg.tunnel_bin.empty() ? default_tunnel_bin() : cfg.tunnel_bin;
  if (!file_executable(bin) && bin.find('/') != std::string::npos) {
    error = "tunnel binary not found: " + bin;
    return false;
  }

  std::vector<std::string> args_store;
  args_store.push_back(bin);
  args_store.push_back("--port");
  args_store.push_back(std::to_string(cfg.port));
  args_store.push_back("--transport");
  args_store.push_back(cfg.transport);
  if (!cfg.iface.empty()) {
    args_store.push_back("--iface");
    args_store.push_back(cfg.iface);
  }
  if (!cfg.use_netns) args_store.push_back("--no-netns");
  if (!cfg.expose_ports.empty()) {
    std::ostringstream oss;
    for (size_t i = 0; i < cfg.expose_ports.size(); ++i) {
      if (i) oss << ',';
      oss << cfg.expose_ports[i];
    }
    args_store.push_back("--expose");
    args_store.push_back(oss.str());
  }

  // Elevate on Linux — TUN/netns need admin.
  const bool use_pkexec = (::geteuid() != 0) && file_executable("/usr/bin/pkexec");

  std::vector<char*> argv;
  std::string pkexec = "/usr/bin/pkexec";
  if (use_pkexec) argv.push_back(pkexec.data());
  for (auto& s : args_store) argv.push_back(s.data());
  argv.push_back(nullptr);

  const pid_t child = ::fork();
  if (child < 0) {
    error = std::string("fork failed: ") + std::strerror(errno);
    return false;
  }
  if (child == 0) {
    ::setsid();
    ::execvp(argv[0], argv.data());
    _exit(127);
  }
  pid_ = child;
  return true;
}

void TunnelProcess::stop() {
  if (pid_ <= 0) return;
  const pid_t p = static_cast<pid_t>(pid_);
  if (::kill(p, SIGTERM) != 0 && errno == EPERM && file_executable("/usr/bin/pkexec")) {
    const std::string cmd = "/usr/bin/pkexec kill " + std::to_string(p);
    (void)::system(cmd.c_str());
  }
  for (int i = 0; i < 50; ++i) {
    int status = 0;
    const pid_t r = ::waitpid(p, &status, WNOHANG);
    if (r == p || (r < 0 && errno == ECHILD)) break;
    if (::kill(p, 0) != 0 && errno == ESRCH) break;
    ::usleep(100000);
  }
  if (::kill(p, 0) == 0 || errno == EPERM) {
    (void)::kill(p, SIGKILL);
  }
  (void)::waitpid(p, nullptr, WNOHANG);
  pid_ = 0;
}

}  // namespace tunnel_tray
