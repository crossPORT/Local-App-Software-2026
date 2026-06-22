#pragma once

#include <chrono>

// Pure decision helpers for fabric presence.
//
// Regression-prone thresholds live here so they can be unit tested without USB
// hardware. The busy-probe fallback exists because a claim-based or lock-based
// probe treated transient USB mutex contention as a disconnect.
namespace fabric_link {

// When the USB layer is too busy to probe (lock contention from the peer
// process or our own transfer), keep the last known presence instead of
// reporting a disconnect.
inline bool presence_when_probe_busy(bool last_known_present) {
    return last_known_present;
}

// Fabric link presence comes from enumeration. Peer names arrive via throttled
// announce session messages (see announce_note + transfer_orchestrator).
inline bool fabric_connected_from_enumeration(bool port_ok) {
    return port_ok;
}

}  // namespace fabric_link
