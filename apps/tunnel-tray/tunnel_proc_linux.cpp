#include "tunnel_proc.hpp"
#include "platform/stats_paths.hpp"
#include "tunnel_args.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "helper/protocol.hpp"

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

bool TunnelProcess::child_exited(int* status_out) const {
  if (pid_ <= 0) return true;
  int status = 0;
  const pid_t r = ::waitpid(static_cast<pid_t>(pid_), &status, WNOHANG);
  if (r == static_cast<pid_t>(pid_)) {
    pid_ = 0;
    if (status_out) *status_out = status;
    return true;
  }
  return false;
}

bool TunnelProcess::tunnel_ready(const TunnelConfig& cfg) const {
  return stats_ready_new_pid(cfg.port, pids_at_start_);
}

bool TunnelProcess::running() const {
  if (helper_managed_) {
    std::string st, herr;
    return tunnel_helper::send_command("STATUS", st, herr) && st.rfind("OK running", 0) == 0;
  }
  if (pid_ <= 0) return false;
  if (child_exited(nullptr)) return false;
  if (::kill(static_cast<pid_t>(pid_), 0) == 0) return true;
  return errno == EPERM;
}

bool TunnelProcess::start(const TunnelConfig& cfg, std::string& error) {
  if (running()) {
    error = "tunnel already running";
    return false;
  }
  const std::string bin = cfg.tunnel_bin.empty() ? default_tunnel_bin() : cfg.tunnel_bin;
  const std::string tail = build_tunnel_arg_tail(cfg, bin);
  pids_at_start_ = snapshot_stats_pids();
  clear_tunnel_stats(cfg.port);

  std::string reply, herr;
  if (tunnel_helper::send_command("START " + tail, reply, herr) && reply.rfind("OK", 0) == 0) {
    helper_managed_ = true;
    port_ = cfg.port;
    for (int i = 0; i < 600; ++i) {
      if (tunnel_ready(cfg)) return true;
      std::string st;
      if (tunnel_helper::send_command("STATUS", st, herr) && st.rfind("OK stopped", 0) == 0) {
        helper_managed_ = false;
        error = "tunnel failed to start (no USB cable or see log)";
        return false;
      }
      ::usleep(100000);
    }
    (void)tunnel_helper::send_command("STOP", reply, herr);
    helper_managed_ = false;
    error = "helper started tunnel but it did not become ready";
    return false;
  }

  std::vector<std::string> args_store;
  args_store.push_back(bin);
  if (cfg.transport != "usb" || cfg.port > 0) {
    args_store.push_back("--port");
    args_store.push_back(std::to_string(cfg.port));
  }
  args_store.push_back("--transport");
  args_store.push_back(cfg.transport);
  if (!cfg.iface.empty()) {
    args_store.push_back("--iface");
    args_store.push_back(cfg.iface);
  }
  if (!cfg.use_netns) args_store.push_back("--no-netns");
  if (!cfg.expose.empty()) {
    std::ostringstream oss;
    for (size_t i = 0; i < cfg.expose.size(); ++i) {
      if (i) oss << ',';
      oss << endpoint_token(cfg.expose[i]);
    }
    args_store.push_back("--expose");
    args_store.push_back(oss.str());
  }

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
  port_ = cfg.port;
  helper_managed_ = false;

  for (int i = 0; i < 600; ++i) {
    if (child_exited(nullptr)) {
      error = "tunnel failed to start (no USB cable, auth cancelled, or see log)";
      return false;
    }
    if (tunnel_ready(cfg)) return true;
    ::usleep(100000);
  }
  error = "tunnel failed to start (no USB cable, auth cancelled, or see log)";
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
  const pid_t p = static_cast<pid_t>(pid_);
  if (::kill(p, SIGTERM) != 0 && errno == EPERM && file_executable("/usr/bin/pkexec")) {
    (void)::system(("/usr/bin/pkexec kill " + std::to_string(p)).c_str());
  }
  for (int i = 0; i < 50; ++i) {
    if (::waitpid(p, nullptr, WNOHANG) == p) break;
    if (::kill(p, 0) != 0 && errno == ESRCH) break;
    ::usleep(100000);
  }
  if (::kill(p, 0) == 0 || errno == EPERM) (void)::kill(p, SIGKILL);
  (void)::waitpid(p, nullptr, WNOHANG);
  pid_ = 0;
}

}  // namespace tunnel_tray
