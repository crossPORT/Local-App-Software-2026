#pragma once

#include <string>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "wintun.h"

struct WinTunFns {
  HMODULE dll = nullptr;
  WINTUN_CREATE_ADAPTER_FUNC* CreateAdapter = nullptr;
  WINTUN_OPEN_ADAPTER_FUNC* OpenAdapter = nullptr;
  WINTUN_CLOSE_ADAPTER_FUNC* CloseAdapter = nullptr;
  WINTUN_GET_ADAPTER_LUID_FUNC* GetAdapterLuid = nullptr;
  WINTUN_START_SESSION_FUNC* StartSession = nullptr;
  WINTUN_END_SESSION_FUNC* EndSession = nullptr;
  WINTUN_GET_READ_WAIT_EVENT_FUNC* GetReadWaitEvent = nullptr;
  WINTUN_RECEIVE_PACKET_FUNC* ReceivePacket = nullptr;
  WINTUN_RELEASE_RECEIVE_PACKET_FUNC* ReleaseReceivePacket = nullptr;
  WINTUN_ALLOCATE_SEND_PACKET_FUNC* AllocateSendPacket = nullptr;
  WINTUN_SEND_PACKET_FUNC* SendPacket = nullptr;
};

/** Load wintun.dll from exe dir (or PATH). Returns false on failure. */
bool wintun_load(WinTunFns& out, std::string& error);

void wintun_unload(WinTunFns& fns);

#endif  // _WIN32
