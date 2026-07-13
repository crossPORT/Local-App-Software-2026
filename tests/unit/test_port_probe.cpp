#include "test_util.h"

#include "rocketbox/port_probe.h"

RB_TEST(leg_from_serial_matches_port_util) {
    CHECK_EQ(rocketbox::leg_from_serial("0000000000000001"), 0);
    CHECK_EQ(rocketbox::leg_from_serial("0000000000000002"), 1);
    CHECK_EQ(rocketbox::leg_from_serial("0000000000000004"), 3);
    CHECK_EQ(rocketbox::leg_from_serial(""), -1);
}

RB_TEST(libusb_index_for_display_port_rejects_bad) {
    CHECK_EQ(rocketbox::libusb_index_for_display_port(0), -1);
    CHECK_EQ(rocketbox::libusb_index_for_display_port(5), -1);
}

RB_TEST(list_present_ports_runs_without_crash) {
    const auto ports = rocketbox::list_present_ports();
    for (const auto& p : ports) {
        CHECK(p.libusb_index >= 0);
        if (p.leg >= 0) {
            CHECK_EQ(p.display_port, p.leg + 1);
        }
    }
}
