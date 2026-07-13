#include "session_dbus.hpp"

#include <cstdlib>
#include <string>

#if !defined(_WIN32)
#include <unistd.h>
#endif

void adopt_session_dbus_env() {
#if !defined(_WIN32)
  if (std::getenv("DBUS_SESSION_BUS_ADDRESS")) return;
  std::string runtime = "/run/user/" + std::to_string(::getuid());
  if (const char* xdg = std::getenv("XDG_RUNTIME_DIR")) runtime = xdg;
  const std::string bus = runtime + "/bus";
  if (::access(bus.c_str(), F_OK) == 0) {
    ::setenv("DBUS_SESSION_BUS_ADDRESS", ("unix:path=" + bus).c_str(), 1);
  }
  if (!std::getenv("XDG_RUNTIME_DIR")) {
    ::setenv("XDG_RUNTIME_DIR", runtime.c_str(), 0);
  }
  if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) {
    ::setenv("DISPLAY", ":0", 0);
  }
#endif
}
