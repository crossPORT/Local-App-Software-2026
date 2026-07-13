#pragma once

#include <functional>
#include <memory>
#include <string>

namespace tunnel_tray {

/** Service name for wx IPC (per-user). */
std::string tray_ipc_service_name();

/** If another tray is running, send ACTIVATE and return true. */
bool activate_existing_tray();

/** Primary instance: listen for ACTIVATE → call on_activate. */
class TrayIpcServer {
public:
  explicit TrayIpcServer(std::function<void()> on_activate);
  ~TrayIpcServer();
  TrayIpcServer(const TrayIpcServer&) = delete;
  TrayIpcServer& operator=(const TrayIpcServer&) = delete;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace tunnel_tray
