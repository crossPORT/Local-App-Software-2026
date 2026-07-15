#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include <string>

namespace rocketbox_tun_win {

void set_ipv4(const NET_LUID& luid, const std::string& local_ip);
void tune_iface(const NET_LUID& luid);
void set_route(const NET_LUID& luid);
void set_fabric_neighbors(const NET_LUID& luid, int local_port);

}  // namespace rocketbox_tun_win
