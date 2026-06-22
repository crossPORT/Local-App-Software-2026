#include "session_config.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string expand_home(std::string path) {
    if (path.size() >= 2 && path[0] == '~' && path[1] == '/') {
        if (const char* home = std::getenv("HOME")) {
            path.replace(0, 1, home);
        }
    }
    return path;
}

bool parse_port_section(const std::string& line, int* port_out) {
    // Minimum valid header is "[port0]" (7 chars).
    if (line.size() < 7 || line.front() != '[' || line.back() != ']') {
        return false;
    }
    const std::string inner = line.substr(1, line.size() - 2);
    if (inner.rfind("port", 0) != 0) {
        return false;
    }
    try {
        *port_out = std::stoi(inner.substr(4));
        return true;
    } catch (...) {
        return false;
    }
}

void set_config_value(SessionConfig& cfg, const std::string& key, const std::string& value) {
    if (key == "source_dir") {
        cfg.source_dir = value;
    } else if (key == "target_dir") {
        cfg.target_dir = value;
    } else if (key == "role") {
        cfg.role = value;
    }
}

}  // namespace

bool load_session_config_file(const std::string& path, int port_index, SessionConfig& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    SessionConfig global{};
    SessionConfig port_cfg{};
    int section_port = -1;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        int parsed_port = -1;
        if (parse_port_section(line, &parsed_port)) {
            section_port = parsed_port;
            continue;
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        const std::string key = trim(line.substr(0, eq));
        const std::string value = expand_home(trim(line.substr(eq + 1)));
        if (section_port < 0) {
            set_config_value(global, key, value);
        } else if (section_port == port_index) {
            set_config_value(port_cfg, key, value);
        }
    }

    out.source_dir = !port_cfg.source_dir.empty() ? port_cfg.source_dir : global.source_dir;
    out.target_dir = !port_cfg.target_dir.empty() ? port_cfg.target_dir : global.target_dir;
    out.role = !port_cfg.role.empty() ? port_cfg.role : global.role;
    out.loaded_from = path;
    return !out.source_dir.empty() || !out.target_dir.empty() || !out.role.empty();
}

std::string resolve_session_config_path(const std::string& cli_path) {
    if (!cli_path.empty()) {
        if (std::filesystem::is_regular_file(cli_path)) {
            return std::filesystem::absolute(cli_path).lexically_normal().string();
        }
    }
    if (const char* env_path = std::getenv("ROCKETBOX_CONFIG")) {
        if (*env_path != '\0') {
            return env_path;
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path user_path =
            std::filesystem::path(home) / ".config" / "sls-fabric" / "session.conf";
        if (std::filesystem::is_regular_file(user_path)) {
            return user_path.string();
        }
    }
    return {};
}

bool load_session_config(int port_index,
                      const std::string& cli_path,
                      SessionConfig& out,
                      std::vector<std::string>* tried_paths) {
    const std::string resolved = resolve_session_config_path(cli_path);
    if (resolved.empty()) {
        return false;
    }
    if (tried_paths) {
        tried_paths->push_back(resolved);
    }
    return load_session_config_file(resolved, port_index, out);
}
