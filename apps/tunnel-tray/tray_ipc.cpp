#include "tray_ipc.hpp"

#include <wx/ipc.h>
#include <wx/utils.h>

namespace tunnel_tray {
namespace {

constexpr const char* kTopic = "RocketBoxTunnelTray";
constexpr const char* kActivate = "ACTIVATE";

class ActivateConnection : public wxConnection {
public:
  explicit ActivateConnection(std::function<void()> on_activate)
      : on_activate_(std::move(on_activate)) {}

  bool OnExec(const wxString& /*topic*/, const wxString& data) override {
    if (data == kActivate && on_activate_) on_activate_();
    return true;
  }

private:
  std::function<void()> on_activate_;
};

class ActivateServer : public wxServer {
public:
  explicit ActivateServer(std::function<void()> on_activate)
      : on_activate_(std::move(on_activate)) {}

  wxConnectionBase* OnAcceptConnection(const wxString& topic) override {
    if (topic != kTopic) return nullptr;
    return new ActivateConnection(on_activate_);
  }

private:
  std::function<void()> on_activate_;
};

}  // namespace

std::string tray_ipc_service_name() {
  return std::string("rocketbox-tunnel-tray-") + wxGetUserId().ToStdString();
}

bool activate_existing_tray() {
  wxClient client;
  wxConnectionBase* conn =
      client.MakeConnection(wxT("localhost"), tray_ipc_service_name(), kTopic);
  if (!conn) return false;
  const bool ok = conn->Execute(kActivate);
  delete conn;
  return ok;
}

struct TrayIpcServer::Impl {
  std::unique_ptr<ActivateServer> server;
};

TrayIpcServer::TrayIpcServer(std::function<void()> on_activate) : impl_(std::make_unique<Impl>()) {
  impl_->server = std::make_unique<ActivateServer>(std::move(on_activate));
  if (!impl_->server->Create(tray_ipc_service_name())) {
    impl_->server.reset();
  }
}

TrayIpcServer::~TrayIpcServer() = default;

}  // namespace tunnel_tray
