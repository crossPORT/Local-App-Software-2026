#include "platform/stats_paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace tunnel_tray {

std::string tunnel_stats_dir() {
#if defined(__linux__)
  return "/run/rocketbox";
#elif defined(__APPLE__)
  if (const char* t = std::getenv("TMPDIR")) {
    return std::string(t) + "rocketbox";
  }
  return "/tmp/rocketbox";
#elif defined(_WIN32)
  if (const char* p = std::getenv("PROGRAMDATA")) {
    return std::string(p) + "\\RocketBox";
  }
  return "C:\\ProgramData\\RocketBox";
#else
  return "/tmp/rocketbox";
#endif
}

std::string tunnel_stats_path(int display_port) {
  return tunnel_stats_dir() + "/tunnel-" + std::to_string(display_port) + ".stats";
}

}  // namespace tunnel_tray
