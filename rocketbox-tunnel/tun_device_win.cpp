#include "tun_device.hpp"
#include "tun_win_net.hpp"
#include "wintun_load.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

struct WinTunCtx {
  WinTunFns fns;
  WINTUN_ADAPTER_HANDLE adapter = nullptr;
  WINTUN_SESSION_HANDLE session = nullptr;
  NET_LUID luid{};
};

WinTunCtx* ctx(void* p) { return static_cast<WinTunCtx*>(p); }

void destroy_ctx(WinTunCtx* c) {
  if (!c) return;
  if (c->session && c->fns.EndSession) c->fns.EndSession(c->session);
  if (c->adapter && c->fns.CloseAdapter) c->fns.CloseAdapter(c->adapter);
  wintun_unload(c->fns);
  delete c;
}

std::wstring to_wide(const std::string& s) {
  if (s.empty()) return L"";
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
  return out;
}

int port_from_fabric_ip(const std::string& local_ip) {
  unsigned a = 0, b = 0, c = 0, d = 0;
  if (sscanf(local_ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return 0;
  if (a != 10 || b != 64 || c != 0 || d < 1 || d > 4) return 0;
  return static_cast<int>(d);
}

}  // namespace

TunDevice::~TunDevice() {
  gateway_.reset();
  close();
}

void TunDevice::open(const std::string& iface_name) {
  close();
  auto* c = new WinTunCtx();
  std::string err;
  if (!wintun_load(c->fns, err)) {
    delete c;
    throw std::runtime_error(err);
  }
  const std::wstring wname = to_wide(iface_name.empty() ? "rb" : iface_name);
  GUID guid = {0x52b0c1a1, 0x0b64, 0x4e01, {0x9a, 0x0a, 0x52, 0x6f, 0x63, 0x6b, 0x42, 0x78}};
  c->adapter = c->fns.OpenAdapter ? c->fns.OpenAdapter(wname.c_str()) : nullptr;
  if (!c->adapter) {
    c->adapter = c->fns.CreateAdapter(wname.c_str(), L"RocketBox", &guid);
  }
  if (!c->adapter) {
    const DWORD e = GetLastError();
    destroy_ctx(c);
    throw std::runtime_error("WintunCreateAdapter failed: " + std::to_string(e) +
                             " (run elevated)");
  }
  c->fns.GetAdapterLuid(c->adapter, &c->luid);
  c->session = c->fns.StartSession(c->adapter, 0x400000);
  if (!c->session) {
    const DWORD e = GetLastError();
    destroy_ctx(c);
    throw std::runtime_error("WintunStartSession failed: " + std::to_string(e));
  }
  win_ = c;
  name_ = iface_name.empty() ? "rb" : iface_name;
  fd_ = 1;
}

void TunDevice::configure_lan(const std::string& local_ip) {
  auto* c = ctx(win_);
  if (!c || !c->session) throw std::runtime_error("TUN not open");
  gateway_.reset();
  rocketbox_tun_win::set_ipv4(c->luid, local_ip);
  rocketbox_tun_win::tune_iface(c->luid);
  rocketbox_tun_win::set_route(c->luid);
  const int port = port_from_fabric_ip(local_ip);
  if (port > 0) rocketbox_tun_win::set_fabric_neighbors(c->luid, port);
}

void TunDevice::isolate_in_netns(const std::string&, const std::string&, int,
                                 const std::vector<ExposeRule>&) {
  throw std::runtime_error("netns isolation is Linux-only; use --no-netns on Windows");
}

void TunDevice::install_expose_filter(int local_port, const std::vector<ExposeRule>& expose) {
  gateway_.reset();
  gateway_ = std::make_unique<HostGateway>();
  gateway_->install(local_port, name_, expose);
}

void TunDevice::reload_expose(const std::vector<ExposeRule>& expose) {
  if (gateway_) gateway_->set_expose(expose);
}

void TunDevice::close() {
  gateway_.reset();
  if (win_) {
    destroy_ctx(ctx(win_));
    win_ = nullptr;
  }
  fd_ = -1;
  name_.clear();
}

void TunDevice::interrupt() { close(); }

std::vector<uint8_t> TunDevice::read_packet() {
  auto* c = ctx(win_);
  if (!c || !c->session) {
    Sleep(50);
    return {};
  }
  DWORD size = 0;
  BYTE* pkt = c->fns.ReceivePacket(c->session, &size);
  if (!pkt) {
    WaitForSingleObject(c->fns.GetReadWaitEvent(c->session), 250);
    return {};
  }
  std::vector<uint8_t> out(pkt, pkt + size);
  c->fns.ReleaseReceivePacket(c->session, pkt);
  return out;
}

void TunDevice::write_packet(const uint8_t* data, size_t len) {
  auto* c = ctx(win_);
  if (!c || !c->session || !data || len == 0 || len > WINTUN_MAX_IP_PACKET_SIZE) return;
  BYTE* pkt = c->fns.AllocateSendPacket(c->session, static_cast<DWORD>(len));
  if (!pkt) return;
  std::memcpy(pkt, data, len);
  c->fns.SendPacket(c->session, pkt);
}
