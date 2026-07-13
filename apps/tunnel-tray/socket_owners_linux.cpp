#include "socket_owners.hpp"

#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <string>
#include <unistd.h>

namespace tunnel_tray {
namespace {

std::string read_comm(int pid) {
  std::ifstream in("/proc/" + std::to_string(pid) + "/comm");
  std::string name;
  std::getline(in, name);
  while (!name.empty() && (name.back() == '\n' || name.back() == '\r')) name.pop_back();
  return name;
}

bool parse_socket_inode(const char* link, unsigned long* inode) {
  // socket:[12345]
  static const char kPrefix[] = "socket:[";
  if (std::strncmp(link, kPrefix, sizeof(kPrefix) - 1) != 0) return false;
  const char* p = link + sizeof(kPrefix) - 1;
  char* end = nullptr;
  const unsigned long n = std::strtoul(p, &end, 10);
  if (!end || *end != ']') return false;
  *inode = n;
  return true;
}

}  // namespace

std::map<unsigned long, std::string> socket_inode_to_process() {
  std::map<unsigned long, std::string> out;
  DIR* proc = ::opendir("/proc");
  if (!proc) return out;
  while (dirent* pe = ::readdir(proc)) {
    if (pe->d_name[0] < '1' || pe->d_name[0] > '9') continue;
    char* end = nullptr;
    const long pid = std::strtol(pe->d_name, &end, 10);
    if (!end || *end != '\0' || pid <= 0) continue;
    const std::string fd_dir = std::string("/proc/") + pe->d_name + "/fd";
    DIR* fd = ::opendir(fd_dir.c_str());
    if (!fd) continue;
    const std::string comm = read_comm(static_cast<int>(pid));
    while (dirent* fe = ::readdir(fd)) {
      if (fe->d_name[0] == '.') continue;
      char link[256];
      const std::string path = fd_dir + "/" + fe->d_name;
      const ssize_t n = ::readlink(path.c_str(), link, sizeof(link) - 1);
      if (n <= 0) continue;
      link[n] = '\0';
      unsigned long inode = 0;
      if (!parse_socket_inode(link, &inode)) continue;
      if (out.find(inode) == out.end() && !comm.empty()) out[inode] = comm;
    }
    ::closedir(fd);
  }
  ::closedir(proc);
  return out;
}

}  // namespace tunnel_tray
