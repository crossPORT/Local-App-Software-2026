#include "test_util.h"

#include "fabric_port.h"

FABRIC_TEST(fabric_leg_from_serial_maps_mod_four) {
    CHECK_EQ(fabric_leg_from_serial("0000000000000001"), 0);
    CHECK_EQ(fabric_leg_from_serial("0000000000000002"), 1);
    CHECK_EQ(fabric_leg_from_serial("0000000000000004"), 3);
}

FABRIC_TEST(fabric_leg_from_serial_rejects_missing) {
    CHECK_EQ(fabric_leg_from_serial(""), -1);
    CHECK_EQ(fabric_leg_from_serial("not-hex"), -1);
}

FABRIC_TEST(default_remote_guess_leg_wraps) {
    CHECK_EQ(default_remote_guess_leg(0), 1);
    CHECK_EQ(default_remote_guess_leg(3), 0);
}

FABRIC_TEST(remote_fabric_legs_excludes_local) {
    const auto legs = remote_fabric_legs(1);
    CHECK_EQ(legs.size(), 3u);
    CHECK_EQ(legs[0], 0);
    CHECK_EQ(legs[1], 2);
    CHECK_EQ(legs[2], 3);
}
