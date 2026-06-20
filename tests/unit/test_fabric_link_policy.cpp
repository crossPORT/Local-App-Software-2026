#include "test_util.h"

#include "fabric_link_policy.h"

using fabric_link::fabric_connected_from_enumeration;
using fabric_link::presence_when_probe_busy;

// --- Busy-probe presence fallback -----------------------------------------
// Regression: when the USB lock is busy (peer process or our own transfer), the
// presence probe must keep the last known state, never report a disconnect.

FABRIC_TEST(presence_busy_keeps_present) {
    CHECK(presence_when_probe_busy(true));
}

FABRIC_TEST(presence_busy_keeps_absent) {
    CHECK(!presence_when_probe_busy(false));
}

// --- Enumeration-only fabric connectivity ---------------------------------
// Regression: periodic announce traffic on the shared medium caused mutual
// starvation (both nodes pause listeners to transmit at once). Connectivity is
// now enumeration-only — no bulk traffic for presence.

FABRIC_TEST(fabric_connected_when_port_enumerated) {
    CHECK(fabric_connected_from_enumeration(true));
}

FABRIC_TEST(fabric_disconnected_when_port_absent) {
    CHECK(!fabric_connected_from_enumeration(false));
}
