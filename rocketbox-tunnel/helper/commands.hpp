#pragma once

#include <string>

namespace tunnel_helper {

/** START|STOP|STATUS|HUP|TERM → OK|ERR … (includes trailing newline). */
std::string handle_line(const std::string& line);

/** Kill child tunnel if still running (helper shutdown). */
void shutdown_child();

}  // namespace tunnel_helper
