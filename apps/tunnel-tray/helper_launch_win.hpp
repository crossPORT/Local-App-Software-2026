#pragma once

#include <string>

namespace tunnel_tray {

std::string default_helper_bin();
bool helper_responds();
/** Elevate helper via UAC if needed. Returns true when STATUS works. */
bool ensure_helper_elevated(std::string& error);

}  // namespace tunnel_tray
