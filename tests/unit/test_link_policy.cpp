#include "test_util.h"

#include "link_policy.h"

using link_policy::usb_connected_from_enumeration;
using link_policy::presence_when_probe_busy;

// --- Busy-probe presence fallback -----------------------------------------
// Regression: when the USB lock is busy (peer process or our own transfer), the
// presence probe must keep the last known state, never report a disconnect.

RB_TEST(presence_busy_keeps_present) {
    CHECK(presence_when_probe_busy(true));
}

RB_TEST(presence_busy_keeps_absent) {
    CHECK(!presence_when_probe_busy(false));
}

// --- Enumeration-only fabric connectivity ---------------------------------
// Regression: periodic announce traffic on the shared medium caused mutual
// starvation (both nodes pause listeners to transmit at once). Connectivity is
// now enumeration-only — no bulk traffic for presence.

RB_TEST(usb_connected_when_port_enumerated) {
    CHECK(usb_connected_from_enumeration(true));
}

RB_TEST(usb_disconnected_when_port_absent) {
    CHECK(!usb_connected_from_enumeration(false));
}
