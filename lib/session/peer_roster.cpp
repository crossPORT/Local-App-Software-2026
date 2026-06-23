#include "peer_roster.h"

#include <chrono>

namespace {

std::string make_peer_id(int port_index, const std::string& name, const std::string& instance_id) {
    if (!instance_id.empty()) {
        return "i:" + instance_id;
    }
    return std::to_string(port_index) + ":" + name;
}

}  // namespace

std::vector<PeerEntry> peer_entries_from_config(const std::vector<PeerConfig>& peers) {
    std::vector<PeerEntry> entries;
    for (const PeerConfig& peer : peers) {
        if (peer.display_name.empty()) {
            continue;
        }
        entries.push_back(PeerEntry{
            make_peer_id(peer.port_index, peer.display_name, ""),
            "",
            peer.display_name,
            peer.team,
            peer.role,
            peer.receive_status,
            peer.port_index,
            false,
            {},
        });
    }
    return entries;
}

void PeerRoster::seed_from_config(const std::vector<PeerConfig>& peers) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_ = peer_entries_from_config(peers);
}

void PeerRoster::touch_peer(const std::string& display_name,
                            const std::string& team,
                            ReceiveStatus status,
                            int port_index,
                            const std::string& instance_id) {
    if (display_name.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    const std::string id = make_peer_id(port_index, display_name, instance_id);

    for (auto it = peers_.begin(); it != peers_.end();) {
        if (it->port_index == port_index && it->id != id) {
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }

    if (!instance_id.empty()) {
        for (PeerEntry& peer : peers_) {
            if (peer.id == id || peer.instance_id == instance_id) {
                peer.display_name = display_name;
                peer.last_seen = now;
                peer.online = true;
                if (!team.empty()) {
                    peer.team = team;
                }
                peer.receive_status = status;
                peer.port_index = port_index;
                peer.instance_id = instance_id;
                peer.id = id;
                return;
            }
        }
        peers_.push_back(PeerEntry{
            id,
            instance_id,
            display_name,
            team,
            "",
            status,
            port_index,
            true,
            now,
        });
        return;
    }

    // Announce is source of truth: one live peer per USB port (legacy announces without instance).
    if (port_index >= 0) {
        for (PeerEntry& peer : peers_) {
            if (peer.port_index == port_index) {
                peer.id = make_peer_id(port_index, display_name, "");
                peer.instance_id.clear();
                peer.display_name = display_name;
                peer.last_seen = now;
                peer.online = true;
                if (!team.empty()) {
                    peer.team = team;
                }
                peer.receive_status = status;
                return;
            }
        }
    }

    for (PeerEntry& peer : peers_) {
        if (peer.display_name == display_name && peer.instance_id.empty()) {
            peer.last_seen = now;
            peer.online = true;
            if (!team.empty()) {
                peer.team = team;
            }
            peer.receive_status = status;
            if (port_index >= 0) {
                peer.port_index = port_index;
                peer.id = make_peer_id(port_index, display_name, "");
            }
            return;
        }
    }

    peers_.push_back(PeerEntry{
        id,
        "",
        display_name,
        team,
        "",
        status,
        port_index >= 0 ? port_index : 0,
        true,
        now,
    });
}

void PeerRoster::touch_peer_presence(const std::string& display_name,
                                     const std::string& team) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    for (PeerEntry& peer : peers_) {
        if (peer.display_name == display_name) {
            peer.last_seen = now;
            peer.online = true;
            if (!team.empty()) {
                peer.team = team;
            }
            return;
        }
    }
}

void PeerRoster::mark_stale_peers_offline(std::chrono::seconds max_age) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    for (PeerEntry& peer : peers_) {
        if (!peer.online || peer.last_seen == std::chrono::steady_clock::time_point{}) {
            peer.online = false;
            continue;
        }
        if (now - peer.last_seen > max_age) {
            peer.online = false;
        }
    }
}

void PeerRoster::set_all_peers_offline() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (PeerEntry& peer : peers_) {
        peer.online = false;
    }
}

void PeerRoster::set_all_peers_online() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    for (PeerEntry& peer : peers_) {
        peer.online = true;
        peer.last_seen = now;
    }
}

void PeerRoster::set_peer_online(int port_index, bool online) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (PeerEntry& peer : peers_) {
        if (peer.port_index == port_index) {
            peer.online = online;
            if (online) {
                peer.last_seen = std::chrono::steady_clock::now();
            }
            return;
        }
    }
}

std::vector<PeerEntry> PeerRoster::peers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_;
}

std::optional<PeerEntry> PeerRoster::find_by_id(const std::string& peer_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const PeerEntry& peer : peers_) {
        if (peer.id == peer_id) {
            return peer;
        }
    }
    return std::nullopt;
}

std::optional<PeerEntry> PeerRoster::find_by_name(const std::string& display_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const PeerEntry& peer : peers_) {
        if (peer.display_name == display_name) {
            return peer;
        }
    }
    return std::nullopt;
}

std::optional<PeerEntry> PeerRoster::find_by_port(int port_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const PeerEntry& peer : peers_) {
        if (peer.port_index == port_index) {
            return peer;
        }
    }
    return std::nullopt;
}
