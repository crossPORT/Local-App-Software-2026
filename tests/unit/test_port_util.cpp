#include "test_util.h"

#include "port_util.h"

RB_TEST(port_index_from_serial_maps_mod_four) {
    CHECK_EQ(port_index_from_serial("0000000000000001"), 0);
    CHECK_EQ(port_index_from_serial("0000000000000002"), 1);
    CHECK_EQ(port_index_from_serial("0000000000000004"), 3);
}

RB_TEST(port_index_from_serial_rejects_missing) {
    CHECK_EQ(port_index_from_serial(""), -1);
    CHECK_EQ(port_index_from_serial("not-hex"), -1);
}

RB_TEST(default_remote_guess_leg_wraps) {
    CHECK_EQ(default_remote_guess_leg(0), 1);
    CHECK_EQ(default_remote_guess_leg(3), 0);
}

RB_TEST(remote_port_indexes_excludes_local) {
    const auto legs = remote_port_indexes(1);
    CHECK_EQ(legs.size(), 3u);
    CHECK_EQ(legs[0], 0);
    CHECK_EQ(legs[1], 2);
    CHECK_EQ(legs[2], 3);
}
