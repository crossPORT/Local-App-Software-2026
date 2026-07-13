#pragma once

#include <string>

namespace tunnel_helper {

constexpr const char* kDefaultSock =
#if defined(_WIN32)
    "\\\\.\\pipe\\rocketbox-tunnel-helper";
#else
    "/run/rocketbox/helper.sock";
#endif

/** Line protocol: START|STOP|STATUS|APPLY <args> → OK|ERR message */
bool send_command(const std::string& line, std::string& reply, std::string& error);

}  // namespace tunnel_helper
