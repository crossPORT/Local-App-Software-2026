#include "helper/commands.hpp"
#include "helper/protocol.hpp"

#include <iostream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace tunnel_helper {
namespace {

SECURITY_ATTRIBUTES* open_pipe_sa() {
  // NULL DACL: unelevated tray can talk to elevated helper on this machine.
  static SECURITY_DESCRIPTOR sd;
  static SECURITY_ATTRIBUTES sa;
  InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
  SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = &sd;
  sa.bInheritHandle = FALSE;
  return &sa;
}

void serve_one_client(HANDLE pipe) {
  char buf[2048];
  DWORD n = 0;
  if (!ReadFile(pipe, buf, sizeof(buf) - 1, &n, nullptr) || n == 0) return;
  buf[n] = '\0';
  std::string line(buf);
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
  const std::string reply = handle_line(line);
  WriteFile(pipe, reply.data(), static_cast<DWORD>(reply.size()), &n, nullptr);
}

}  // namespace

void serve_named_pipe() {
  std::cerr << "[rocketbox-tunnel-helper] listening on " << kDefaultSock << "\n";
  for (;;) {
    HANDLE pipe =
        CreateNamedPipeA(kDefaultSock, PIPE_ACCESS_DUPLEX,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 2048, 2048, 0,
                         open_pipe_sa());
    if (pipe == INVALID_HANDLE_VALUE) {
      std::cerr << "[rocketbox-tunnel-helper] CreateNamedPipe failed: " << GetLastError() << "\n";
      Sleep(1000);
      continue;
    }
    if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
      CloseHandle(pipe);
      continue;
    }
    serve_one_client(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
  }
}

}  // namespace tunnel_helper
