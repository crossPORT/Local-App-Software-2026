#include "tunnel_proc.hpp"
#include "helper_launch.hpp"
#include "platform/stats_paths.hpp"
#include "tray_start_msg.hpp"
#include "tunnel_args.hpp"
#include "tunnel_hup.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

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

bool helper_term(long pid) {
  if (pid <= 0) return false;
  std::string reply, herr;
  return tunnel_helper::send_command("TERM " + std::to_string(pid), reply, herr) &&
         reply.rfind("OK", 0) == 0;
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
  // Helper owns the elevated child across tray restarts — always ask STATUS.
  std::string st, herr;
  if (tunnel_helper::send_command("STATUS", st, herr) && st.rfind("OK running", 0) == 0) {
    helper_managed_ = true;
    return true;
  }
  if (helper_managed_) helper_managed_ = false;
  if (live_tunnel_port() > 0) return true;
  if (pid_ <= 0) return false;
  if (child_exited(nullptr)) return false;
  if (::kill(static_cast<pid_t>(pid_), 0) == 0) return true;
  return errno == EPERM;
}

bool TunnelProcess::start(const TunnelConfig& cfg, std::string& error) {
  if (running()) {
    // Live tunnel (helper child or orphan) — adopt into UI; HUP expose best-effort.
    const int live = live_tunnel_port();
    TunnelConfig adopted = cfg;
    if (live > 0) {
      adopted.port = live;
      port_ = live;
    }
    std::string st, herr;
    helper_managed_ =
        tunnel_helper::send_command("STATUS", st, herr) && st.rfind("OK running", 0) == 0;
    std::string ignore;
    (void)reload(adopted, ignore);
    return true;
  }
  if (cfg.port >= 1 && cfg.port <= 4 && live_tunnel_holds_port(cfg.port)) {
    return reload(cfg, error);
  }
  if (!ensure_helper_elevated(error)) return false;

  const std::string bin = cfg.tunnel_bin.empty() ? default_tunnel_bin() : cfg.tunnel_bin;
  const std::string tail = build_tunnel_arg_tail(cfg, bin);
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
      return true;
    }
    if (error.rfind("ERR ", 0) == 0) error.erase(0, 4);
    return false;
  }
  helper_managed_ = true;
  port_ = cfg.port;
  for (int i = 0; i < 600; ++i) {
    if (tunnel_ready(cfg)) return true;
    std::string st;
    if (tunnel_helper::send_command("STATUS", st, herr) && st.rfind("OK stopped", 0) == 0) {
      helper_managed_ = false;
      error = start_failed_msg(cfg);
      return false;
    }
    ::usleep(100000);
  }
  (void)tunnel_helper::send_command("STOP", reply, herr);
  helper_managed_ = false;
  error = "helper started tunnel but it did not become ready";
  return false;
}

void TunnelProcess::stop() {
  std::string reply, herr;
  (void)tunnel_helper::send_command("STOP", reply, herr);
  helper_managed_ = false;
  // Reap any live tunnel from stats (root orphan, or STOP against a stale helper).
  for (int round = 0; round < 3; ++round) {
    bool any = false;
    for (int p = 1; p <= 4; ++p) {
      if (!live_tunnel_holds_port(p)) continue;
      any = true;
      const long orphan = read_tunnel_rates(p).pid;
      if (orphan <= 0) continue;
      if (!helper_term(orphan)) {
        std::string err;
        if (ensure_helper_elevated(err)) (void)helper_term(orphan);
      }
    }
    if (!any) break;
    for (int i = 0; i < 20; ++i) {
      if (live_tunnel_port() == 0) break;
      ::usleep(100000);
    }
  }
  if (pid_ > 0) {
    const long p = pid_;
    if (!helper_term(p)) {
      std::string err;
      if (ensure_helper_elevated(err)) (void)helper_term(p);
      else if (::kill(static_cast<pid_t>(p), SIGTERM) != 0 && errno == EPERM &&
               file_executable("/usr/bin/pkexec")) {
        const int rc =
            ::system(("/usr/bin/pkexec kill -9 " + std::to_string(p)).c_str());
        (void)rc;
      }
    }
  }
  pid_ = 0;
  clear_tunnel_stats(0);
}

}  // namespace tunnel_tray
