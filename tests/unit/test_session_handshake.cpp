#include "session_handshake.h"

#include "test_util.h"

FABRIC_TEST(handshake_timing_defaults) {
    IdentityProfile profile;
    profile.display_name = "Alice";
    const HandshakeTiming timing = handshake_timing_from_identity(profile);
    CHECK_EQ(timing.accept_ready_gap_ms, 400u);
    CHECK_EQ(timing.accept_reply_delay_ms, 800u);
    CHECK_EQ(timing.accept_timeout_sec, 60u);
    CHECK_EQ(timing.accept_dialog_sec, 57u);
    CHECK_EQ(timing.ready_timeout_sec, 20u);
    CHECK_EQ(timing.session_header_timeout_ms, 2000u);
    CHECK_EQ(timing.payload_header_timeout_ms, 15000u);
}

FABRIC_TEST(handshake_timing_from_config) {
    IdentityProfile profile;
    profile.display_name = "Bob";
    profile.accept_ready_gap_ms = 200;
    profile.accept_reply_delay_ms = 500;
    profile.accept_timeout_sec = 30;
    profile.ready_timeout_sec = 10;
    profile.session_header_timeout_ms = 1500;
    profile.payload_header_timeout_ms = 12000;
    const HandshakeTiming timing = handshake_timing_from_identity(profile);
    CHECK_EQ(timing.accept_ready_gap_ms, 200u);
    CHECK_EQ(timing.accept_reply_delay_ms, 500u);
    CHECK_EQ(timing.accept_timeout_sec, 30u);
    CHECK_EQ(timing.accept_dialog_sec, 27u);
    CHECK_EQ(timing.ready_timeout_sec, 10u);
    CHECK_EQ(timing.session_header_timeout_ms, 1500u);
    CHECK_EQ(timing.payload_header_timeout_ms, 12000u);
}

FABRIC_TEST(handshake_reply_delay_defaults_to_double_gap) {
    IdentityProfile profile;
    profile.accept_ready_gap_ms = 250;
    const HandshakeTiming timing = handshake_timing_from_identity(profile);
    CHECK_EQ(timing.accept_ready_gap_ms, 250u);
    CHECK_EQ(timing.accept_reply_delay_ms, 500u);
}
