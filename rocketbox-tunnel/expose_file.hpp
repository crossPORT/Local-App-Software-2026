#pragma once

#include "expose_spec.hpp"

#include <string>
#include <vector>

/** Path: /tmp/rocketbox/tunnel-N.expose (survives pkexec; tray + tunnel share it). */
std::string rocketbox_expose_path(int display_port);

void write_expose_file(int display_port, const std::vector<ExposeRule>& rules);
std::vector<ExposeRule> read_expose_file(int display_port);
