#pragma once

#include <string>
#include <vector>

struct SessionConfig {
    std::string source_dir;
    std::string target_dir;
    std::string role;  // "receiver" | "sender" (empty = default for port index)
    std::string loaded_from;
};

// Parse key=value config. Optional [portN] sections override globals.
bool load_session_config_file(const std::string& path, int port_index, SessionConfig& out);

// Resolve config path: CLI > ROCKETBOX_CONFIG > ~/.config/sls-fabric/session.conf
std::string resolve_session_config_path(const std::string& cli_path);

// Load first config file found on the search path.
bool load_session_config(int port_index,
                      const std::string& cli_path,
                      SessionConfig& out,
                      std::vector<std::string>* tried_paths = nullptr);
