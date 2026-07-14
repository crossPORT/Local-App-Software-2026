#pragma once

#include <string>

namespace tunnel_tray {

std::string default_helper_bin();
bool helper_responds();
/** pkexec once to start root helper; later START/STOP/HUP use the socket. */
bool ensure_helper_elevated(std::string& error);

}  // namespace tunnel_tray
