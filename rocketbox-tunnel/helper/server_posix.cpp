#include "helper/commands.hpp"
#include "helper/protocol.hpp"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace tunnel_helper {

void serve_unix() {
  std::error_code ec;
  std::filesystem::create_directories("/run/rocketbox", ec);
  ::unlink(kDefaultSock);
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    std::cerr << "helper socket failed\n";
    return;
  }
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, kDefaultSock, sizeof(addr.sun_path) - 1);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "helper bind failed\n";
    ::close(fd);
    return;
  }
  ::chmod(kDefaultSock, 0666);
  ::listen(fd, 4);
  std::cerr << "[rocketbox-tunnel-helper] listening on " << kDefaultSock << "\n";
  for (;;) {
    const int c = ::accept(fd, nullptr, nullptr);
    if (c < 0) continue;
    char buf[1024];
    const ssize_t n = ::read(c, buf, sizeof(buf) - 1);
    if (n > 0) {
      buf[n] = '\0';
      std::string line(buf);
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
      const std::string reply = handle_line(line);
      ::write(c, reply.data(), reply.size());
    }
    ::close(c);
  }
}

}  // namespace tunnel_helper
