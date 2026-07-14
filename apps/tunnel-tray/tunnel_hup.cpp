#include "tunnel_hup.hpp"

#include "helper/protocol.hpp"
#include "helper_launch.hpp"

#include <cerrno>
#include <cstring>
#include <csignal>
#include <unistd.h>

namespace tunnel_tray {
namespace {

bool helper_hup(const std::string& cmd) {
  std::string reply, herr;
  return tunnel_helper::send_command(cmd, reply, herr) && reply.rfind("OK", 0) == 0;
}

}  // namespace

bool signal_tunnel_hup(long pid, std::string& error) {
  if (helper_hup("HUP")) return true;
  if (pid > 0 && helper_hup("HUP " + std::to_string(pid))) return true;

  std::string elev_err;
  if (ensure_helper_elevated(elev_err)) {
    if (helper_hup("HUP")) return true;
    if (pid > 0 && helper_hup("HUP " + std::to_string(pid))) return true;
  }

  if (pid <= 0) {
    error = elev_err.empty() ? "no tunnel pid to reload" : elev_err;
    return false;
  }
#if defined(SIGHUP)
  if (::kill(static_cast<pid_t>(pid), SIGHUP) == 0) return true;
  if (errno != EPERM) {
    error = std::string("kill -HUP failed: ") + std::strerror(errno);
    return false;
  }
#endif
  error = elev_err.empty() ? "could not SIGHUP tunnel (start helper once)" : elev_err;
  return false;
}

}  // namespace tunnel_tray
