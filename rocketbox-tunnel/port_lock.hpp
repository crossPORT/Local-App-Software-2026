#pragma once

#include <string>

/** Exclusive per-Port lock so only one rocketbox-tunnel bridges a given Port. */
class TunnelPortLock {
public:
  TunnelPortLock() = default;
  ~TunnelPortLock();
  TunnelPortLock(const TunnelPortLock&) = delete;
  TunnelPortLock& operator=(const TunnelPortLock&) = delete;

  /** Returns false and sets error if another live process holds the lock. */
  bool try_acquire(int display_port, std::string& error);
  void release();

private:
  int port_ = 0;
#if defined(_WIN32)
  void* handle_ = nullptr;  // HANDLE
#else
  int fd_ = -1;
#endif
};

std::string rocketbox_tunnel_lock_path(int display_port);
