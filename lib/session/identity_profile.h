#pragma once

#include <string>
#include <vector>

enum class ReceiveStatus {
    Open,
    AskFirst,
    Busy,
};

struct PeerConfig {
    std::string display_name;
    std::string team;
    std::string role;
    ReceiveStatus receive_status = ReceiveStatus::Open;
    int port_index = 0;
};

struct IdentityProfile {
    std::string display_name;
    std::string team;
    std::string role;
    ReceiveStatus receive_status = ReceiveStatus::AskFirst;
    std::string receive_folder;
    // Per-chunk payload stall timeout in ms; 0 = use core default (kFileTimeoutMs).
    int transfer_timeout_ms = 0;
    // Per-process in-flight buffer budget in MB; 0 = auto-detect from usbfs limit.
    int usb_inflight_mb = 0;
    // Session handshake tuning; 0 = use usb_protocol defaults (see session_handshake.h).
    int accept_ready_gap_ms = 0;
    int accept_reply_delay_ms = 0;
    int accept_timeout_sec = 0;
    int ready_timeout_sec = 0;
    int session_header_timeout_ms = 0;
    int payload_header_timeout_ms = 0;
    // When > 0, the transfer UI shows this MiB/s for live, peak, and average speed
    // instead of measured USB throughput. For trade-show booth displays only; does not change
    // the real transfer engine. Example: 7168 ≈ 7 GiB/s.
    double booth_display_mib_s = 0.0;
    // ±percent jitter applied once per transfer run (e.g. 3 => 6972..7364 for 7168 base).
    double booth_display_jitter_pct = 0.0;
    std::vector<PeerConfig> peers;
    std::string config_path;
};

std::string receive_status_to_string(ReceiveStatus status);
ReceiveStatus receive_status_from_string(const std::string& value);

bool load_identity_profile(int port_index,
                           const std::string& cli_config_path,
                           IdentityProfile& out);

bool save_identity_profile(const IdentityProfile& profile);
