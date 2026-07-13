#include "tun_device.hpp"
#include "wintun_load.hpp"

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

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

void set_ipv4(const NET_LUID& luid, const std::string& local_ip) {
  unsigned a = 0, b = 0, c = 0, d = 0;
  if (sscanf(local_ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
    throw std::runtime_error("bad local IP: " + local_ip);
  }
  MIB_UNICASTIPADDRESS_ROW row{};
  InitializeUnicastIpAddressEntry(&row);
  row.InterfaceLuid = luid;
  row.Address.Ipv4.sin_family = AF_INET;
  row.Address.Ipv4.sin_addr.S_un.S_addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
  row.OnLinkPrefixLength = 24;
  row.DadState = IpDadStatePreferred;
  const DWORD r = CreateUnicastIpAddressEntry(&row);
  if (r != NO_ERROR && r != ERROR_OBJECT_ALREADY_EXISTS) {
    throw std::runtime_error("CreateUnicastIpAddressEntry failed: " + std::to_string(r));
  }
}

void set_route(const NET_LUID& luid) {
  MIB_IPFORWARD_ROW2 row{};
  InitializeIpForwardEntry(&row);
  row.InterfaceLuid = luid;
  row.DestinationPrefix.Prefix.si_family = AF_INET;
  row.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
  row.DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr = htonl(0x0A400000);  // 10.64.0.0
  row.DestinationPrefix.PrefixLength = 24;
  row.NextHop.Ipv4.sin_family = AF_INET;
  row.NextHop.Ipv4.sin_addr.S_un.S_addr = 0;
  row.Metric = 1;
  const DWORD r = CreateIpForwardEntry2(&row);
  if (r != NO_ERROR && r != ERROR_OBJECT_ALREADY_EXISTS) {
    throw std::runtime_error("CreateIpForwardEntry2 failed: " + std::to_string(r));
  }
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
  c->adapter = c->fns.CreateAdapter(wname.c_str(), L"RocketBox", &guid);
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
  set_ipv4(c->luid, local_ip);
  set_route(c->luid);
}

void TunDevice::isolate_in_netns(const std::string&, const std::string&, int,
                                 const std::vector<ExposeRule>&) {
  throw std::runtime_error("netns isolation is Linux-only; use --no-netns on Windows");
}

void TunDevice::close() {
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
  if (!c || !c->session) return {};
  DWORD size = 0;
  BYTE* pkt = c->fns.ReceivePacket(c->session, &size);
  if (!pkt) {
    if (GetLastError() == ERROR_NO_MORE_ITEMS) {
      WaitForSingleObject(c->fns.GetReadWaitEvent(c->session), 100);
    }
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
