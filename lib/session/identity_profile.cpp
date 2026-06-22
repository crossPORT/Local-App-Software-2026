#include "identity_profile.h"

#include "platform_util.h"
#include "session_config.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

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
    return platform::expand_home(std::move(path));
}

bool parse_peer_section(const std::string& line, int* peer_index) {
    if (line.size() < 7 || line.front() != '[' || line.back() != ']') {
        return false;
    }
    const std::string inner = line.substr(1, line.size() - 2);
    if (inner.rfind("peer", 0) != 0) {
        return false;
    }
    try {
        *peer_index = std::stoi(inner.substr(4));
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_port_section(const std::string& line, int* port_out) {
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

void apply_identity_key(IdentityProfile& cfg, const std::string& key, const std::string& value) {
    if (key == "display_name") {
        cfg.display_name = value;
    } else if (key == "team") {
        cfg.team = value;
    } else if (key == "role") {
        cfg.role = value;
    } else if (key == "receive_status") {
        cfg.receive_status = receive_status_from_string(value);
    } else if (key == "receive_folder" || key == "target_dir") {
        cfg.receive_folder = value;
    } else if (key == "transfer_timeout_ms") {
        try {
            cfg.transfer_timeout_ms = std::stoi(value);
        } catch (...) {
            cfg.transfer_timeout_ms = 0;
        }
    } else if (key == "usb_inflight_mb") {
        try {
            cfg.usb_inflight_mb = std::stoi(value);
        } catch (...) {
            cfg.usb_inflight_mb = 0;
        }
    } else if (key == "accept_ready_gap_ms") {
        try {
            cfg.accept_ready_gap_ms = std::max(0, std::stoi(value));
        } catch (...) {
            cfg.accept_ready_gap_ms = 0;
        }
    } else if (key == "accept_reply_delay_ms") {
        try {
            cfg.accept_reply_delay_ms = std::max(0, std::stoi(value));
        } catch (...) {
            cfg.accept_reply_delay_ms = 0;
        }
    } else if (key == "accept_timeout_sec") {
        try {
            cfg.accept_timeout_sec = std::max(0, std::stoi(value));
        } catch (...) {
            cfg.accept_timeout_sec = 0;
        }
    } else if (key == "ready_timeout_sec") {
        try {
            cfg.ready_timeout_sec = std::max(0, std::stoi(value));
        } catch (...) {
            cfg.ready_timeout_sec = 0;
        }
    } else if (key == "session_header_timeout_ms") {
        try {
            cfg.session_header_timeout_ms = std::max(0, std::stoi(value));
        } catch (...) {
            cfg.session_header_timeout_ms = 0;
        }
    } else if (key == "payload_header_timeout_ms") {
        try {
            cfg.payload_header_timeout_ms = std::max(0, std::stoi(value));
        } catch (...) {
            cfg.payload_header_timeout_ms = 0;
        }
    } else if (key == "booth_display_mib_s") {
        try {
            cfg.booth_display_mib_s = std::stod(value);
            if (cfg.booth_display_mib_s < 0.0) {
                cfg.booth_display_mib_s = 0.0;
            }
        } catch (...) {
            cfg.booth_display_mib_s = 0.0;
        }
    } else if (key == "booth_display_jitter_pct") {
        try {
            cfg.booth_display_jitter_pct = std::stod(value);
            if (cfg.booth_display_jitter_pct < 0.0) {
                cfg.booth_display_jitter_pct = 0.0;
            }
        } catch (...) {
            cfg.booth_display_jitter_pct = 0.0;
        }
    } else if (key == "source_dir") {
        (void)value;
    }
}

void apply_peer_key(PeerConfig& peer, const std::string& key, const std::string& value) {
    if (key == "display_name") {
        peer.display_name = value;
    } else if (key == "team") {
        peer.team = value;
    } else if (key == "role") {
        peer.role = value;
    } else if (key == "receive_status") {
        peer.receive_status = receive_status_from_string(value);
    } else if (key == "port_index") {
        peer.port_index = std::stoi(value);
    }
}

bool load_profile_file(const std::string& path, int port_index, IdentityProfile& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    IdentityProfile global{};
    IdentityProfile port_cfg{};
    PeerConfig current_peer{};
    bool in_peer = false;
    int section_port = -1;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        int peer_index = -1;
        if (parse_peer_section(line, &peer_index)) {
            if (in_peer && !current_peer.display_name.empty()) {
                global.peers.push_back(current_peer);
            }
            current_peer = PeerConfig{};
            current_peer.port_index = peer_index;
            in_peer = true;
            section_port = -1;
            continue;
        }

        int parsed_port = -1;
        if (parse_port_section(line, &parsed_port)) {
            if (in_peer && !current_peer.display_name.empty()) {
                global.peers.push_back(current_peer);
            }
            in_peer = false;
            section_port = parsed_port;
            continue;
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        const std::string key = trim(line.substr(0, eq));
        const std::string value = expand_home(trim(line.substr(eq + 1)));

        if (in_peer) {
            apply_peer_key(current_peer, key, value);
        } else if (section_port < 0) {
            apply_identity_key(global, key, value);
        } else if (section_port == port_index) {
            apply_identity_key(port_cfg, key, value);
        }
    }

    if (in_peer && !current_peer.display_name.empty()) {
        global.peers.push_back(current_peer);
    }

    out.display_name = !port_cfg.display_name.empty() ? port_cfg.display_name : global.display_name;
    out.team = !port_cfg.team.empty() ? port_cfg.team : global.team;
    out.role = !port_cfg.role.empty() ? port_cfg.role : global.role;
    out.receive_status = !port_cfg.display_name.empty() ? port_cfg.receive_status : global.receive_status;
    if (out.receive_status == ReceiveStatus::AskFirst && !global.display_name.empty()
        && port_cfg.display_name.empty()) {
        out.receive_status = global.receive_status;
    }
    out.receive_folder = !port_cfg.receive_folder.empty() ? port_cfg.receive_folder
                        : !global.receive_folder.empty() ? global.receive_folder
                        : expand_home("~/Incoming");
    out.transfer_timeout_ms = port_cfg.transfer_timeout_ms != 0
                                  ? port_cfg.transfer_timeout_ms
                                  : global.transfer_timeout_ms;
    out.usb_inflight_mb = port_cfg.usb_inflight_mb != 0
                              ? port_cfg.usb_inflight_mb
                              : global.usb_inflight_mb;
    out.accept_ready_gap_ms = port_cfg.accept_ready_gap_ms != 0
                                  ? port_cfg.accept_ready_gap_ms
                                  : global.accept_ready_gap_ms;
    out.accept_reply_delay_ms = port_cfg.accept_reply_delay_ms != 0
                                    ? port_cfg.accept_reply_delay_ms
                                    : global.accept_reply_delay_ms;
    out.accept_timeout_sec = port_cfg.accept_timeout_sec != 0
                                 ? port_cfg.accept_timeout_sec
                                 : global.accept_timeout_sec;
    out.ready_timeout_sec = port_cfg.ready_timeout_sec != 0
                                ? port_cfg.ready_timeout_sec
                                : global.ready_timeout_sec;
    out.session_header_timeout_ms = port_cfg.session_header_timeout_ms != 0
                                        ? port_cfg.session_header_timeout_ms
                                        : global.session_header_timeout_ms;
    out.payload_header_timeout_ms = port_cfg.payload_header_timeout_ms != 0
                                          ? port_cfg.payload_header_timeout_ms
                                          : global.payload_header_timeout_ms;
    out.booth_display_mib_s = port_cfg.booth_display_mib_s > 0.0
                                 ? port_cfg.booth_display_mib_s
                                 : global.booth_display_mib_s;
    out.booth_display_jitter_pct = port_cfg.booth_display_jitter_pct > 0.0
                                      ? port_cfg.booth_display_jitter_pct
                                      : global.booth_display_jitter_pct;
    out.peers = global.peers;
    out.config_path = path;
    return !out.display_name.empty() || !out.peers.empty();
}

}  // namespace

std::string receive_status_to_string(ReceiveStatus status) {
    switch (status) {
        case ReceiveStatus::Open:
            return "open";
        case ReceiveStatus::AskFirst:
            return "ask_first";
        case ReceiveStatus::Busy:
            return "busy";
    }
    return "ask_first";
}

ReceiveStatus receive_status_from_string(const std::string& value) {
    if (value == "open" || value == "auto" || value == "auto_accept") {
        return ReceiveStatus::Open;
    }
    if (value == "busy") {
        return ReceiveStatus::Busy;
    }
    return ReceiveStatus::AskFirst;
}

bool load_identity_profile(int port_index,
                           const std::string& cli_config_path,
                           IdentityProfile& out) {
    SessionConfig legacy{};
    const std::string path = resolve_session_config_path(cli_config_path);
    if (!path.empty()) {
        load_session_config_file(path, port_index, legacy);
    }

    IdentityProfile profile{};
    if (!path.empty()) {
        load_profile_file(path, port_index, profile);
    }

    if (profile.display_name.empty() && !legacy.role.empty()) {
        profile.display_name = "System-" + std::to_string(port_index);
    }
    if (profile.team.empty()) {
        profile.team = "Team";
    }
    if (!legacy.target_dir.empty() && profile.receive_folder == expand_home("~/Incoming")) {
        profile.receive_folder = legacy.target_dir;
    }
    if (!legacy.role.empty()) {
        if (legacy.role == "sender") {
            profile.receive_status = ReceiveStatus::Open;
        }
    }

    if (profile.display_name.empty()) {
        profile.display_name = "System-" + std::to_string(port_index);
    }
    if (profile.receive_folder.empty()) {
        profile.receive_folder = expand_home("~/Incoming");
    }
    profile.config_path = path;

    out = std::move(profile);
    return true;
}

bool save_identity_profile(const IdentityProfile& profile) {
    if (profile.config_path.empty()) {
        return false;
    }

    std::filesystem::path path(profile.config_path);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(profile.config_path, std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << "display_name=" << profile.display_name << '\n';
    file << "team=" << profile.team << '\n';
    if (!profile.role.empty()) {
        file << "role=" << profile.role << '\n';
    }
    file << "receive_status=" << receive_status_to_string(profile.receive_status) << '\n';
    file << "receive_folder=" << profile.receive_folder << '\n';
    if (profile.transfer_timeout_ms > 0) {
        file << "transfer_timeout_ms=" << profile.transfer_timeout_ms << '\n';
    }
    if (profile.usb_inflight_mb > 0) {
        file << "usb_inflight_mb=" << profile.usb_inflight_mb << '\n';
    }
    if (profile.accept_ready_gap_ms > 0) {
        file << "accept_ready_gap_ms=" << profile.accept_ready_gap_ms << '\n';
    }
    if (profile.accept_reply_delay_ms > 0) {
        file << "accept_reply_delay_ms=" << profile.accept_reply_delay_ms << '\n';
    }
    if (profile.accept_timeout_sec > 0) {
        file << "accept_timeout_sec=" << profile.accept_timeout_sec << '\n';
    }
    if (profile.ready_timeout_sec > 0) {
        file << "ready_timeout_sec=" << profile.ready_timeout_sec << '\n';
    }
    if (profile.session_header_timeout_ms > 0) {
        file << "session_header_timeout_ms=" << profile.session_header_timeout_ms << '\n';
    }
    if (profile.payload_header_timeout_ms > 0) {
        file << "payload_header_timeout_ms=" << profile.payload_header_timeout_ms << '\n';
    }
    if (profile.booth_display_mib_s > 0.0) {
        file << "booth_display_mib_s=" << profile.booth_display_mib_s << '\n';
    }
    if (profile.booth_display_jitter_pct > 0.0) {
        file << "booth_display_jitter_pct=" << profile.booth_display_jitter_pct << '\n';
    }

    for (std::size_t i = 0; i < profile.peers.size(); ++i) {
        const PeerConfig& peer = profile.peers[i];
        file << "\n[peer" << i << "]\n";
        file << "display_name=" << peer.display_name << '\n';
        file << "team=" << peer.team << '\n';
        if (!peer.role.empty()) {
            file << "role=" << peer.role << '\n';
        }
        file << "receive_status=" << receive_status_to_string(peer.receive_status) << '\n';
        file << "port_index=" << peer.port_index << '\n';
    }

    return true;
}
