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
#if defined(_WIN32)
  return tunnel_stats_dir() + "\\tunnel-" + std::to_string(display_port) + ".stats";
#else
  return tunnel_stats_dir() + "/tunnel-" + std::to_string(display_port) + ".stats";
#endif
}

void clear_tunnel_stats(int display_port) {
  auto drop = [](int p) {
    if (p < 1 || p > 4) return;
    std::error_code ec;
    std::filesystem::remove(tunnel_stats_path(p), ec);
  };
  if (display_port >= 1 && display_port <= 4) {
    drop(display_port);
    return;
  }
  for (int p = 1; p <= 4; ++p) drop(p);
}

}  // namespace tunnel_tray
