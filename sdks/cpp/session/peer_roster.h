#pragma once

#include "identity_profile.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct PeerEntry {
    std::string id;
    std::string instance_id;
    std::string display_name;
    std::string team;
    std::string role;
    ReceiveStatus receive_status = ReceiveStatus::Open;
    int port_index = 0;
    bool online = true;
    std::chrono::steady_clock::time_point last_seen{};
};

std::vector<PeerEntry> peer_entries_from_config(const std::vector<PeerConfig>& peers);

class PeerRoster {
public:
    void seed_from_config(const std::vector<PeerConfig>& peers);
    void touch_peer(const std::string& display_name,
                    const std::string& team,
                    ReceiveStatus status,
                    int port_index,
                    const std::string& instance_id = "");
    void touch_peer_presence(const std::string& display_name, const std::string& team);
    void mark_stale_peers_offline(std::chrono::seconds max_age);
    void set_all_peers_offline();
    void set_all_peers_online();
    void set_peer_online(int port_index, bool online);

    std::vector<PeerEntry> peers() const;
    std::optional<PeerEntry> find_by_id(const std::string& peer_id) const;
    std::optional<PeerEntry> find_by_name(const std::string& display_name) const;
    std::optional<PeerEntry> find_by_port(int port_index) const;

private:
    mutable std::mutex mutex_;
    std::vector<PeerEntry> peers_;
};
