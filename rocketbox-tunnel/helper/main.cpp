#include "helper/commands.hpp"
#include "helper/server.hpp"

#include <csignal>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#if defined(_WIN32)
BOOL WINAPI on_console_ctrl(DWORD) {
  tunnel_helper::shutdown_child();
  return FALSE;
}
#else
void on_signal(int) { tunnel_helper::shutdown_child(); }
#endif

}  // namespace

int main() {
#if defined(_WIN32)
  SetConsoleCtrlHandler(on_console_ctrl, TRUE);
  tunnel_helper::serve_named_pipe();
#else
  std::signal(SIGTERM, on_signal);
  std::signal(SIGINT, on_signal);
  tunnel_helper::serve_unix();
#endif
  tunnel_helper::shutdown_child();
  return 0;
}
