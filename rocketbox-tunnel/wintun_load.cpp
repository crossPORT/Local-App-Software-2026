#include "wintun_load.hpp"

#if defined(_WIN32)

#include <string>

namespace {

std::wstring exe_dir() {
  wchar_t buf[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return L".";
  std::wstring path(buf, n);
  const auto slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return L".";
  return path.substr(0, slash);
}

}  // namespace

bool wintun_load(WinTunFns& out, std::string& error) {
  wintun_unload(out);
  const std::wstring path = exe_dir() + L"\\wintun.dll";
  out.dll = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
  if (!out.dll) out.dll = LoadLibraryW(L"wintun.dll");
  if (!out.dll) {
    error = "wintun.dll not found beside rocketbox-tunnel.exe (reinstall Tunnel component)";
    return false;
  }
#define RB_WINTUN_LOAD(name, type)                                                                 \
  do {                                                                                             \
    out.name = reinterpret_cast<type>(GetProcAddress(out.dll, "Wintun" #name));                    \
    if (!out.name) {                                                                               \
      error = "wintun.dll missing Wintun" #name;                                                   \
      wintun_unload(out);                                                                          \
      return false;                                                                                \
    }                                                                                              \
  } while (0)

  // Map short names to Wintun* exports.
  out.CreateAdapter =
      reinterpret_cast<WINTUN_CREATE_ADAPTER_FUNC*>(GetProcAddress(out.dll, "WintunCreateAdapter"));
  out.OpenAdapter =
      reinterpret_cast<WINTUN_OPEN_ADAPTER_FUNC*>(GetProcAddress(out.dll, "WintunOpenAdapter"));
  out.CloseAdapter =
      reinterpret_cast<WINTUN_CLOSE_ADAPTER_FUNC*>(GetProcAddress(out.dll, "WintunCloseAdapter"));
  out.GetAdapterLuid = reinterpret_cast<WINTUN_GET_ADAPTER_LUID_FUNC*>(
      GetProcAddress(out.dll, "WintunGetAdapterLUID"));
  out.StartSession =
      reinterpret_cast<WINTUN_START_SESSION_FUNC*>(GetProcAddress(out.dll, "WintunStartSession"));
  out.EndSession =
      reinterpret_cast<WINTUN_END_SESSION_FUNC*>(GetProcAddress(out.dll, "WintunEndSession"));
  out.GetReadWaitEvent = reinterpret_cast<WINTUN_GET_READ_WAIT_EVENT_FUNC*>(
      GetProcAddress(out.dll, "WintunGetReadWaitEvent"));
  out.ReceivePacket =
      reinterpret_cast<WINTUN_RECEIVE_PACKET_FUNC*>(GetProcAddress(out.dll, "WintunReceivePacket"));
  out.ReleaseReceivePacket = reinterpret_cast<WINTUN_RELEASE_RECEIVE_PACKET_FUNC*>(
      GetProcAddress(out.dll, "WintunReleaseReceivePacket"));
  out.AllocateSendPacket = reinterpret_cast<WINTUN_ALLOCATE_SEND_PACKET_FUNC*>(
      GetProcAddress(out.dll, "WintunAllocateSendPacket"));
  out.SendPacket =
      reinterpret_cast<WINTUN_SEND_PACKET_FUNC*>(GetProcAddress(out.dll, "WintunSendPacket"));
#undef RB_WINTUN_LOAD

  if (!out.CreateAdapter || !out.CloseAdapter || !out.GetAdapterLuid || !out.StartSession ||
      !out.EndSession || !out.GetReadWaitEvent || !out.ReceivePacket || !out.ReleaseReceivePacket ||
      !out.AllocateSendPacket || !out.SendPacket) {
    error = "wintun.dll is missing required exports";
    wintun_unload(out);
    return false;
  }
  return true;
}

void wintun_unload(WinTunFns& fns) {
  if (fns.dll) FreeLibrary(fns.dll);
  fns = {};
}

#endif  // _WIN32
