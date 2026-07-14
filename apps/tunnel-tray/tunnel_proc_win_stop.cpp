#include "tunnel_proc.hpp"
#include "helper_launch_win.hpp"

#include <chrono>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "helper/protocol.hpp"

namespace tunnel_tray {
namespace {

bool helper_term(long pid) {
  if (pid <= 0) return false;
  std::string reply, herr;
  return tunnel_helper::send_command("TERM " + std::to_string(pid), reply, herr) &&
         reply.rfind("OK", 0) == 0;
}

}  // namespace

void TunnelProcess::stop() {
  std::string reply, herr;
  (void)tunnel_helper::send_command("STOP", reply, herr);
  helper_managed_ = false;
  for (int p = 1; p <= 4; ++p) {
    if (!live_tunnel_holds_port(p)) continue;
    const long orphan = read_tunnel_rates(p).pid;
    if (orphan <= 0) continue;
    if (!helper_term(orphan)) {
      std::string err;
      if (ensure_helper_elevated(err)) (void)helper_term(orphan);
    }
    for (int i = 0; i < 50; ++i) {
      if (!live_tunnel_holds_port(p)) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  if (pid_ > 0) {
    if (!helper_term(pid_)) {
      HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid_));
      if (h) {
        TerminateProcess(h, 1);
        WaitForSingleObject(h, 5000);
        CloseHandle(h);
      }
    }
    pid_ = 0;
  }
}

}  // namespace tunnel_tray
