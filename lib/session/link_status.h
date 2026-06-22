#pragma once

#include "identity_profile.h"

#include <chrono>
#include <cstdint>
#include <string>

enum class StatusColour {
    Accent,
    Ok,
    Warn,
    Error,
    Muted,
};

struct StatusLine {
    std::string text;
    StatusColour colour = StatusColour::Muted;
};

struct ConnectBannerLine {
    std::string text;
    bool visible = false;
};

constexpr int64_t kAnnounceIntervalMs = 15000;
constexpr int64_t kPeerStaleMs = 45000;
constexpr int64_t kTransferDoneDismissMs = 2000;

std::string receive_status_label(ReceiveStatus status);

StatusLine status_line_native(bool fabric_connected,
                              int fabric_devices_seen,
                              int port_index,
                              bool busy,
                              bool waiting_for_partner,
                              const std::string& status_message,
                              bool identity_configured,
                              bool peers_configured);

ConnectBannerLine connect_banner_native(bool fabric_connected, int port_index);

std::string peer_expires_label(std::chrono::steady_clock::time_point last_seen,
                               std::chrono::steady_clock::time_point now);

int64_t next_announce_in_ms(int64_t last_announce_ms,
                            int64_t now_ms,
                            int64_t interval_ms = kAnnounceIntervalMs);

std::string format_mbps_rate(double mbps);

std::string transfer_done_message(bool sent, double mbps);

std::string transfer_live_message(bool sent, double mbps);
