#include "helper/protocol.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem>
#endif

namespace {

#if !defined(_WIN32)
int g_child = 0;

std::string handle_line(const std::string& line) {
  if (line.rfind("START ", 0) == 0) {
    if (g_child > 0) return "ERR already running\n";
    const std::string args = line.substr(6);
    const pid_t pid = ::fork();
    if (pid == 0) {
      // Child: exec rocketbox-tunnel with remaining tokens via /bin/sh -c
      const std::string cmd = "rocketbox-tunnel " + args;
      ::execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }
    if (pid < 0) return "ERR fork failed\n";
    g_child = static_cast<int>(pid);
    return "OK started\n";
  }
  if (line == "STOP") {
    if (g_child > 0) {
      ::kill(g_child, SIGTERM);
      ::waitpid(g_child, nullptr, 0);
      g_child = 0;
    }
    return "OK stopped\n";
  }
  if (line == "STATUS") {
    if (g_child <= 0) return "OK stopped\n";
    if (::kill(g_child, 0) == 0) return "OK running\n";
    g_child = 0;
    return "OK stopped\n";
  }
  return "ERR unknown\n";
}

void serve_unix() {
  std::error_code ec;
  std::filesystem::create_directories("/run/rocketbox", ec);
  ::unlink(tunnel_helper::kDefaultSock);
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    std::cerr << "helper socket failed\n";
    return;
  }
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, tunnel_helper::kDefaultSock, sizeof(addr.sun_path) - 1);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "helper bind failed\n";
    ::close(fd);
    return;
  }
  ::chmod(tunnel_helper::kDefaultSock, 0660);
  ::listen(fd, 4);
  std::cerr << "[rocketbox-tunnel-helper] listening on " << tunnel_helper::kDefaultSock << "\n";
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
#endif

}  // namespace

int main() {
#if defined(_WIN32)
  std::cerr << "rocketbox-tunnel-helper: Windows named-pipe server not enabled in this build\n";
  return 1;
#else
  serve_unix();
  return 0;
#endif
}
