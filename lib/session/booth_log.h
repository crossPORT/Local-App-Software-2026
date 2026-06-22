#pragma once

#include <cstddef>
#include <string>

// Timestamped append-only log for booth troubleshooting.
// Default path: /tmp/slsfabric-booth.log (override with SLSFABRIC_LOG env).

void booth_log_set_path(const std::string& path);
std::string booth_log_path();
void booth_log_clear();
std::string read_booth_log_tail(std::size_t max_lines = 400);

void booth_log(int port, const std::string& event, const std::string& detail = {});
