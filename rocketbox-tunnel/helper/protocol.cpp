#include "helper/protocol.hpp"

#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace tunnel_helper {

bool send_command(const std::string& line, std::string& reply, std::string& error) {
  reply.clear();
#if defined(_WIN32)
  (void)WaitNamedPipeA(kDefaultSock, 500);
  HANDLE h = CreateFileA(kDefaultSock, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                         nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    error = "helper pipe not available (is rocketbox-tunnel-helper running?)";
    return false;
  }
  DWORD n = 0;
  const std::string msg = line + "\n";
  if (!WriteFile(h, msg.data(), static_cast<DWORD>(msg.size()), &n, nullptr)) {
    CloseHandle(h);
    error = "helper write failed";
    return false;
  }
  char buf[512];
  if (!ReadFile(h, buf, sizeof(buf) - 1, &n, nullptr) || n == 0) {
    CloseHandle(h);
    error = "helper read failed";
    return false;
  }
  buf[n] = '\0';
  reply = buf;
  CloseHandle(h);
  return true;
#else
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    error = "socket failed";
    return false;
  }
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, kDefaultSock, sizeof(addr.sun_path) - 1);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    error = "helper not available (start rocketbox-tunnel-helper or use pkexec)";
    return false;
  }
  const std::string msg = line + "\n";
  if (::write(fd, msg.data(), msg.size()) < 0) {
    ::close(fd);
    error = "helper write failed";
    return false;
  }
  char buf[512];
  const ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
  ::close(fd);
  if (n <= 0) {
    error = "helper read failed";
    return false;
  }
  buf[n] = '\0';
  reply = buf;
  return true;
#endif
}

}  // namespace tunnel_helper
