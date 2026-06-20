#pragma once

#include <string>
#include <vector>

struct DemoConfig {
    std::string source_dir;
    std::string target_dir;
    std::string role;  // "receiver" | "sender" (empty = default for port index)
    std::string loaded_from;
};

// Parse key=value config. Optional [portN] sections override globals.
bool load_demo_config_file(const std::string& path, int port_index, DemoConfig& out);

// Resolve config path: CLI > CES_DEMO_CONFIG > ./ces-demo.conf > ~/.config/sls-fabric/demo.conf
std::string resolve_demo_config_path(const std::string& cli_path);

// Load first config file found on the search path.
bool load_demo_config(int port_index,
                      const std::string& cli_path,
                      DemoConfig& out,
                      std::vector<std::string>* tried_paths = nullptr);
