#include "tun_device.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include <cstdio>
#include <stdexcept>
#include <string>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace rocketbox_tun_win {
namespace {

UINT32 ipv4_u32(unsigned a, unsigned b, unsigned c, unsigned d) {
  return (a << 24) | (b << 16) | (c << 8) | d;
}

}  // namespace

void set_ipv4(const NET_LUID& luid, const std::string& local_ip) {
  unsigned a = 0, b = 0, c = 0, d = 0;
  if (sscanf(local_ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
    throw std::runtime_error("bad local IP: " + local_ip);
  }
  MIB_UNICASTIPADDRESS_ROW row{};
  InitializeUnicastIpAddressEntry(&row);
  row.InterfaceLuid = luid;
  row.Address.Ipv4.sin_family = AF_INET;
  row.Address.Ipv4.sin_addr.S_un.S_addr = htonl(ipv4_u32(a, b, c, d));
  row.OnLinkPrefixLength = 24;
  row.DadState = IpDadStatePreferred;
  row.SkipAsSource = 0;
  const DWORD r = CreateUnicastIpAddressEntry(&row);
  if (r != NO_ERROR && r != ERROR_OBJECT_ALREADY_EXISTS) {
    throw std::runtime_error("CreateUnicastIpAddressEntry failed: " + std::to_string(r));
  }
}

void tune_iface(const NET_LUID& luid) {
  MIB_IPINTERFACE_ROW row{};
  InitializeIpInterfaceEntry(&row);
  row.Family = AF_INET;
  row.InterfaceLuid = luid;
  if (GetIpInterfaceEntry(&row) != NO_ERROR) return;
  row.UseAutomaticMetric = FALSE;
  row.Metric = 1;
  row.DisableDefaultRoutes = TRUE;
  row.WeakHostSend = TRUE;
  row.WeakHostReceive = TRUE;
  (void)SetIpInterfaceEntry(&row);
}

void set_route(const NET_LUID& luid) {
  MIB_IPFORWARD_ROW2 row{};
  InitializeIpForwardEntry(&row);
  row.InterfaceLuid = luid;
  row.DestinationPrefix.Prefix.si_family = AF_INET;
  row.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
  row.DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr = htonl(0x0A400000);
  row.DestinationPrefix.PrefixLength = 24;
  row.NextHop.Ipv4.sin_family = AF_INET;
  row.NextHop.Ipv4.sin_addr.S_un.S_addr = 0;
  row.Metric = 1;
  const DWORD r = CreateIpForwardEntry2(&row);
  if (r != NO_ERROR && r != ERROR_OBJECT_ALREADY_EXISTS) {
    throw std::runtime_error("CreateIpForwardEntry2 failed: " + std::to_string(r));
  }
}

/** Permanent neighbors so Win ping works before any inbound ARP from peers. */
void set_fabric_neighbors(const NET_LUID& luid, int local_port) {
  for (int p = 1; p <= 4; ++p) {
    if (p == local_port) continue;
    MIB_IPNET_ROW2 row{};
    row.Address.si_family = AF_INET;
    row.Address.Ipv4.sin_family = AF_INET;
    row.Address.Ipv4.sin_addr.S_un.S_addr = htonl(ipv4_u32(10, 64, 0, static_cast<unsigned>(p)));
    row.InterfaceLuid = luid;
    row.PhysicalAddressLength = 6;
    row.PhysicalAddress[0] = 0x02;
    row.PhysicalAddress[1] = 0x64;
    row.PhysicalAddress[2] = 0x0a;
    row.PhysicalAddress[3] = 0x40;
    row.PhysicalAddress[4] = 0x00;
    row.PhysicalAddress[5] = static_cast<UCHAR>(p);
    row.State = NlnsPermanent;
    const DWORD r = CreateIpNetEntry2(&row);
    if (r != NO_ERROR && r != ERROR_OBJECT_ALREADY_EXISTS) {
      (void)SetIpNetEntry2(&row);
    }
  }
}

}  // namespace rocketbox_tun_win
