#include "tunnel_log.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

std::string rocketbox_tunnel_log_path() {
  if (const char* e = std::getenv("ROCKETBOX_TUNNEL_LOG")) {
    if (e[0] != '\0') return e;
  }
#if defined(_WIN32)
  if (const char* t = std::getenv("TEMP")) {
    if (t[0] != '\0') return std::string(t) + "\\rocketbox\\tunnel.log";
  }
  return "C:\\Windows\\Temp\\rocketbox\\tunnel.log";
#else
  // Shared path so a root tunnel (pkexec) and user tray see the same file.
  return "/tmp/rocketbox/tunnel.log";
#endif
}

void rocketbox_tunnel_log(const std::string& line) {
  std::cerr << "[rocketbox-tunnel] " << line << std::endl;
  try {
    const auto path = rocketbox_tunnel_log_path();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    using clock = std::chrono::system_clock;
    const auto t = clock::to_time_t(clock::now());
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ofstream out(path, std::ios::app);
    if (!out) return;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " " << line << "\n";
#if !defined(_WIN32)
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
            std::filesystem::perms::group_read | std::filesystem::perms::group_write |
            std::filesystem::perms::others_read | std::filesystem::perms::others_write,
        std::filesystem::perm_options::replace, ec);
#endif
  } catch (...) {
  }
}

void rocketbox_tunnel_enable_event_stderr() {
  if (std::getenv("ROCKETBOX_LOG_STDERR")) return;
#if defined(_WIN32)
  _putenv_s("ROCKETBOX_LOG_STDERR", "1");
#else
  setenv("ROCKETBOX_LOG_STDERR", "1", 0);
#endif
}
