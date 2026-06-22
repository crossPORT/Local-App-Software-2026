#include "link_status.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

std::string receive_status_label(ReceiveStatus status) {
    switch (status) {
        case ReceiveStatus::Open:
            return "Saves files automatically";
        case ReceiveStatus::Busy:
            return "Not accepting files";
        case ReceiveStatus::AskFirst:
        default:
            return "Asks before saving";
    }
}

StatusLine status_line_native(bool fabric_connected,
                              int fabric_devices_seen,
                              int port_index,
                              bool busy,
                              bool waiting_for_partner,
                              const std::string& status_message,
                              bool identity_configured,
                              bool peers_configured) {
    if ((busy || waiting_for_partner) && !status_message.empty()) {
        return {status_message, StatusColour::Accent};
    }
    if (fabric_connected && identity_configured && !peers_configured) {
        return {"USB connected — waiting for peers", StatusColour::Warn};
    }
    if (fabric_connected) {
        return {"USB connected", StatusColour::Ok};
    }
    if (fabric_devices_seen == 0) {
        return {"Plug in your USB cable", StatusColour::Warn};
    }
    return {"Waiting for device…", StatusColour::Warn};
}

ConnectBannerLine connect_banner_native(bool fabric_connected, int /*port_index*/) {
    if (fabric_connected) {
        return {"", false};
    }
    return {"Plug in your USB cable, then choose it when prompted", true};
}

std::string peer_expires_label(std::chrono::steady_clock::time_point last_seen,
                               std::chrono::steady_clock::time_point now) {
    if (last_seen == std::chrono::steady_clock::time_point{}) {
        return {};
    }
    const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_seen)
                            .count();
    const int64_t remaining_ms = kPeerStaleMs - age_ms;
    if (remaining_ms <= 0) {
        return "expiring…";
    }
    const int64_t seconds = (remaining_ms + 999) / 1000;
    std::ostringstream out;
    out << "expires in " << seconds << "s";
    return out.str();
}

int64_t next_announce_in_ms(int64_t last_announce_ms,
                            int64_t now_ms,
                            int64_t interval_ms) {
    if (last_announce_ms <= 0) {
        return 0;
    }
    return std::max<int64_t>(0, interval_ms - (now_ms - last_announce_ms));
}

std::string format_mbps_rate(double mbps) {
    if (mbps >= 1024.0) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << (mbps / 1024.0) << " GiB/s";
        return out.str();
    }
    if (mbps >= 100.0) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(0) << mbps << " MiB/s";
        return out.str();
    }
    if (mbps >= 1.0) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(1) << mbps << " MiB/s";
        return out.str();
    }
    if (mbps > 0.0) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(0) << (mbps * 1024.0) << " KiB/s";
        return out.str();
    }
    return {};
}

std::string transfer_done_message(bool sent, double mbps) {
    if (mbps <= 0.0) {
        return sent ? "Sent" : "Received";
    }
    return (sent ? "Sent at " : "Received at ") + format_mbps_rate(mbps);
}

std::string transfer_live_message(bool sent, double mbps) {
    if (mbps <= 0.0) {
        return sent ? "Sending…" : "Receiving…";
    }
    return (sent ? "Sending at " : "Receiving at ") + format_mbps_rate(mbps);
}
