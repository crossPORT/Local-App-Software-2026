#pragma once

#include <cstddef>
#include <string>

// Timestamped append-only log for session troubleshooting.
// Default path: /tmp/rocketbox-event.log (override with ROCKETBOX_LOG env).

void event_log_set_path(const std::string& path);
std::string event_log_path();
void event_log_clear();
std::string read_event_log_tail(std::size_t max_lines = 400);

void event_log(int port, const std::string& event, const std::string& detail = {});
