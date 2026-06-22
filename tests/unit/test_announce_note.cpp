#include "test_util.h"

#include "announce_note.h"

FABRIC_TEST(announce_note_round_trip) {
    const std::string note = build_announce_note(1, ReceiveStatus::Open);
    CHECK_STREQ(note, "port=1;receive=open");

    int port = -1;
    ReceiveStatus status = ReceiveStatus::Busy;
    CHECK(parse_announce_note(note, 0, &port, &status));
    CHECK_EQ(port, 1);
    CHECK(status == ReceiveStatus::Open);
}

FABRIC_TEST(announce_note_defaults_when_missing) {
    int port = 0;
    ReceiveStatus status = ReceiveStatus::Open;
    CHECK(parse_announce_note("", 1, &port, &status));
    CHECK_EQ(port, 1);
    CHECK(status == ReceiveStatus::AskFirst);
}

FABRIC_TEST(announce_note_parses_receive_only) {
    int port = 2;
    ReceiveStatus status = ReceiveStatus::AskFirst;
    CHECK(parse_announce_note("receive=busy", 2, &port, &status));
    CHECK_EQ(port, 2);
    CHECK(status == ReceiveStatus::Busy);
}
