#include "port_lock.hpp"
#include "stats_path.hpp"

#include <cstdio>
#include <fstream>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#endif

std::string rocketbox_tunnel_lock_path(int display_port) {
#if defined(_WIN32)
  return rocketbox_tunnel_stats_dir() + "\\tunnel-" + std::to_string(display_port) + ".lock";
#else
  return rocketbox_tunnel_stats_dir() + "/tunnel-" + std::to_string(display_port) + ".lock";
#endif
}

TunnelPortLock::~TunnelPortLock() { release(); }

#if defined(_WIN32)

bool TunnelPortLock::try_acquire(int display_port, std::string& error) {
  release();
  const std::string path = rocketbox_tunnel_lock_path(display_port);
  CreateDirectoryA(rocketbox_tunnel_stats_dir().c_str(), nullptr);
  HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    error = "Port " + std::to_string(display_port) + " tunnel already running (lock busy)";
    return false;
  }
  const DWORD pid = GetCurrentProcessId();
  DWORD n = 0;
  WriteFile(h, &pid, sizeof(pid), &n, nullptr);
  handle_ = h;
  port_ = display_port;
  return true;
}

void TunnelPortLock::release() {
  if (handle_) {
    CloseHandle(static_cast<HANDLE>(handle_));
    handle_ = nullptr;
  }
  port_ = 0;
}

#else

bool TunnelPortLock::try_acquire(int display_port, std::string& error) {
  release();
  std::error_code ec;
  std::filesystem::create_directories(rocketbox_tunnel_stats_dir(), ec);
  const std::string path = rocketbox_tunnel_lock_path(display_port);
  const int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd < 0) {
    error = std::string("lock open failed: ") + std::strerror(errno);
    return false;
  }
  if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
    ::close(fd);
    error = "Port " + std::to_string(display_port) + " tunnel already running";
    return false;
  }
  (void)::ftruncate(fd, 0);
  const std::string pid = std::to_string(::getpid()) + "\n";
  (void)::write(fd, pid.data(), pid.size());
  fd_ = fd;
  port_ = display_port;
  return true;
}

void TunnelPortLock::release() {
  if (fd_ >= 0) {
    (void)::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
  }
  port_ = 0;
}

#endif
