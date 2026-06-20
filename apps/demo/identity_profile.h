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
    std::vector<PeerConfig> peers;
    std::string config_path;
};

std::string receive_status_to_string(ReceiveStatus status);
ReceiveStatus receive_status_from_string(const std::string& value);

bool load_identity_profile(int port_index,
                           const std::string& cli_config_path,
                           IdentityProfile& out);

bool save_identity_profile(const IdentityProfile& profile);
