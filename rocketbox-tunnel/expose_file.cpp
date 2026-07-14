#include "expose_file.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

std::string rocketbox_expose_path(int display_port) {
  return "/tmp/rocketbox/tunnel-" + std::to_string(display_port) + ".expose";
}

void write_expose_file(int display_port, const std::vector<ExposeRule>& rules) {
  if (display_port < 1 || display_port > 4) return;
  std::error_code ec;
  std::filesystem::create_directories("/tmp/rocketbox", ec);
  const auto path = rocketbox_expose_path(display_port);
  std::ofstream out(path, std::ios::trunc);
  if (!out) return;
  for (size_t i = 0; i < rules.size(); ++i) {
    if (i) out << ',';
    if (rules[i].tcp && rules[i].udp) {
      out << rules[i].port;
    } else if (rules[i].tcp) {
      out << "tcp:" << rules[i].port;
    } else if (rules[i].udp) {
      out << "udp:" << rules[i].port;
    }
  }
  out << '\n';
  out.close();
  std::filesystem::permissions(
      path,
      std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
          std::filesystem::perms::group_write | std::filesystem::perms::others_read |
          std::filesystem::perms::others_write,
      std::filesystem::perm_options::replace, ec);
}

std::vector<ExposeRule> read_expose_file(int display_port) {
  std::vector<ExposeRule> out;
  std::ifstream in(rocketbox_expose_path(display_port));
  if (!in) return out;
  std::string line;
  if (!std::getline(in, line)) return out;
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
  if (line.empty()) return out;
  try {
    parse_expose_list(line, out);
  } catch (...) {
    out.clear();
  }
  return out;
}
