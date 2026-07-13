#pragma once

#include <map>
#include <string>

namespace tunnel_tray {

/** Map socket inode → process comm (best-effort; first match wins). */
std::map<unsigned long, std::string> socket_inode_to_process();

}  // namespace tunnel_tray
