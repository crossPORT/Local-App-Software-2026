#include "test_util.h"

#include "session_role.h"

FABRIC_TEST(session_role_from_string_variants) {
    CHECK(session_role_from_string("sender") == SessionRole::Sender);
    CHECK(session_role_from_string("send") == SessionRole::Sender);
    CHECK(session_role_from_string("SENDER") == SessionRole::Sender);
    CHECK(session_role_from_string("receiver") == SessionRole::Receiver);
    CHECK(session_role_from_string("anything-else") == SessionRole::Receiver);
}

FABRIC_TEST(session_role_to_string_round_trip) {
    CHECK_STREQ(session_role_to_string(SessionRole::Sender), "sender");
    CHECK_STREQ(session_role_to_string(SessionRole::Receiver), "receiver");
    CHECK(session_role_from_string(session_role_to_string(SessionRole::Sender))
          == SessionRole::Sender);
}

FABRIC_TEST(session_role_default_by_port) {
    CHECK(default_session_role_for_port(0) == SessionRole::Receiver);
    CHECK(default_session_role_for_port(1) == SessionRole::Sender);
    CHECK(default_session_role_for_port(2) == SessionRole::Receiver);
}
