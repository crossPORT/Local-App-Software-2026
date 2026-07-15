#include "tray_settings.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace tunnel_tray {
namespace {

std::filesystem::path config_path() {
  const char* home = std::getenv("HOME");
#if defined(_WIN32)
  if (!home) home = std::getenv("USERPROFILE");
#endif
  if (!home) return {};
  return std::filesystem::path(home) / ".config" / "rocketbox" / "tunnel-tray.conf";
}

}  // namespace

TraySettings load_tray_settings() {
  TraySettings s;
  const auto path = config_path();
  if (path.empty()) return s;
  std::ifstream in(path);
  if (!in) return s;
  std::string line;
  while (std::getline(in, line)) {
    if (line.compare(0, 7, "expose=") == 0) {
      std::stringstream ss(line.substr(7));
      std::string part;
      while (std::getline(ss, part, ',')) {
        Endpoint ep;
        if (parse_endpoint_token(part, ep)) s.expose.push_back(ep);
      }
    } else if (line.compare(0, 5, "port=") == 0) {
      try {
        s.port = std::stoi(line.substr(5));
      } catch (...) {
      }
    } else if (line.compare(0, 4, "usb=") == 0) {
      s.usb = line.substr(4) != "0";
    } else if (line.compare(0, 8, "enabled=") == 0) {
      s.enabled = line.substr(8) == "1" || line.substr(8) == "true";
    } else if (line.compare(0, 19, "ep4_dynamic_switch=") == 0) {
      s.ep4_dynamic_switch =
          line.substr(19) == "1" || line.substr(19) == "true";
    }
  }
  if (s.port < 1 || s.port > 4) s.port = 1;
  return s;
}

void save_tray_settings(const TraySettings& s) {
  const auto path = config_path();
  if (path.empty()) return;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path);
  if (!out) return;
  out << "port=" << s.port << "\n";
  out << "usb=" << (s.usb ? 1 : 0) << "\n";
  out << "enabled=" << (s.enabled ? 1 : 0) << "\n";
  out << "ep4_dynamic_switch=" << (s.ep4_dynamic_switch ? 1 : 0) << "\n";
  out << "expose=";
  for (size_t i = 0; i < s.expose.size(); ++i) {
    if (i) out << ',';
    out << endpoint_token(s.expose[i]);
  }
  out << '\n';
}

}  // namespace tunnel_tray
