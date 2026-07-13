#include "sim_net.hpp"

#include <cstring>
#include <string>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace rocketbox {
namespace detail {

bool rb_net_init() {
#if defined(_WIN32)
  static bool ok = false;
  static bool tried = false;
  if (tried) return ok;
  tried = true;
  WSADATA wsa{};
  ok = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
  return ok;
#else
  return true;
#endif
}

void rb_net_fini() {
#if defined(_WIN32)
  // Keep Winsock for process lifetime; optional cleanup omitted.
#endif
}

rb_sock_t rb_connect_loopback(uint16_t port) {
  if (!rb_net_init()) return RB_SOCK_INVALID;
  rb_sock_t fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd == RB_SOCK_INVALID) return RB_SOCK_INVALID;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
#if defined(_WIN32)
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
#else
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
#endif
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    rb_sock_close(fd);
    return RB_SOCK_INVALID;
  }
  return fd;
}

void rb_sock_close(rb_sock_t& fd) {
  if (fd == RB_SOCK_INVALID) return;
#if defined(_WIN32)
  ::closesocket(fd);
#else
  ::close(fd);
#endif
  fd = RB_SOCK_INVALID;
}

void rb_sock_shutdown(rb_sock_t fd) {
  if (fd == RB_SOCK_INVALID) return;
#if defined(_WIN32)
  ::shutdown(fd, SD_BOTH);
#else
  ::shutdown(fd, SHUT_RDWR);
#endif
}

int rb_sock_read(rb_sock_t fd, void* buf, size_t len) {
  if (fd == RB_SOCK_INVALID || !buf || len == 0) return -1;
#if defined(_WIN32)
  const int n = ::recv(fd, static_cast<char*>(buf), static_cast<int>(len), 0);
#else
  const auto n = ::read(fd, buf, len);
#endif
  return static_cast<int>(n);
}

int rb_sock_write(rb_sock_t fd, const void* buf, size_t len) {
  if (fd == RB_SOCK_INVALID || !buf || len == 0) return -1;
#if defined(_WIN32)
  const int n = ::send(fd, static_cast<const char*>(buf), static_cast<int>(len), 0);
#else
  const auto n = ::write(fd, buf, len);
#endif
  return static_cast<int>(n);
}

}  // namespace detail
}  // namespace rocketbox
