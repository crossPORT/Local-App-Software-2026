#include "stats_path.hpp"

#include <cstdlib>
#include <string>

std::string rocketbox_tunnel_stats_dir() {
#if defined(__linux__)
  return "/run/rocketbox";
#elif defined(__APPLE__)
  if (const char* t = std::getenv("TMPDIR")) return std::string(t) + "rocketbox";
  return "/tmp/rocketbox";
#elif defined(_WIN32)
  if (const char* p = std::getenv("PROGRAMDATA")) return std::string(p) + "\\RocketBox";
  return "C:\\ProgramData\\RocketBox";
#else
  return "/tmp/rocketbox";
#endif
}

std::string rocketbox_tunnel_stats_path(int display_port) {
#if defined(_WIN32)
  return rocketbox_tunnel_stats_dir() + "\\tunnel-" + std::to_string(display_port) + ".stats";
#else
  return rocketbox_tunnel_stats_dir() + "/tunnel-" + std::to_string(display_port) + ".stats";
#endif
}
