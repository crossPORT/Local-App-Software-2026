#include "test_util.h"

#include "link_status.h"

#include <chrono>

FABRIC_TEST(link_status_busy_message) {
    const StatusLine line = status_line_native(
        false, 0, 0, true, false, "Sending to Bob…", true, true);
    CHECK_STREQ(line.text, "Sending to Bob…");
    CHECK(line.colour == StatusColour::Accent);
}

FABRIC_TEST(link_status_connected_waiting_for_peers) {
    const StatusLine line = status_line_native(
        true, 1, 0, false, false, {}, true, false);
    CHECK_STREQ(line.text, "USB connected — waiting for peers");
    CHECK(line.colour == StatusColour::Warn);
}

FABRIC_TEST(link_status_connected) {
    const StatusLine line = status_line_native(
        true, 1, 0, false, false, {}, true, true);
    CHECK_STREQ(line.text, "USB connected");
    CHECK(line.colour == StatusColour::Ok);
}

FABRIC_TEST(link_status_no_cable) {
    const StatusLine line = status_line_native(
        false, 0, 0, false, false, {}, true, true);
    CHECK_STREQ(line.text, "Plug in your USB cable");
    CHECK(line.colour == StatusColour::Warn);
}

FABRIC_TEST(link_status_handshake_pending) {
    const StatusLine line = status_line_native(
        false, 1, 0, false, false, {}, true, true);
    CHECK_STREQ(line.text, "Waiting for device…");
    CHECK(line.colour == StatusColour::Warn);
}

FABRIC_TEST(link_status_peer_expires_countdown) {
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    const auto last = now - std::chrono::seconds(13);
    const std::string label = peer_expires_label(last, now);
    CHECK(label.find("expires in") != std::string::npos);
    CHECK(label.find("32s") != std::string::npos);
}

FABRIC_TEST(link_status_peer_expiring) {
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    const auto last = now - std::chrono::seconds(46);
    CHECK_STREQ(peer_expires_label(last, now), "expiring…");
}

FABRIC_TEST(link_status_next_announce) {
    CHECK_EQ(next_announce_in_ms(1000, 9000, 15000), 7000);
    CHECK_EQ(next_announce_in_ms(1000, 20000, 15000), 0);
}

FABRIC_TEST(link_status_receive_labels) {
    CHECK_STREQ(receive_status_label(ReceiveStatus::Open), "Saves files automatically");
    CHECK_STREQ(receive_status_label(ReceiveStatus::AskFirst), "Asks before saving");
    CHECK_STREQ(receive_status_label(ReceiveStatus::Busy), "Not accepting files");
}

FABRIC_TEST(link_status_transfer_done_message) {
    CHECK_STREQ(transfer_done_message(true, 12.0 / 1024.0), "Sent at 12 KiB/s");
    CHECK_STREQ(transfer_done_message(false, 12.0 / 1024.0), "Received at 12 KiB/s");
    CHECK_STREQ(transfer_done_message(true, 0.0), "Sent");
    CHECK_STREQ(transfer_done_message(false, 0.0), "Received");
}

FABRIC_TEST(link_status_transfer_live_message) {
    CHECK_STREQ(transfer_live_message(true, 12.0 / 1024.0), "Sending at 12 KiB/s");
    CHECK_STREQ(transfer_live_message(false, 12.0 / 1024.0), "Receiving at 12 KiB/s");
    CHECK_STREQ(transfer_live_message(true, 0.0), "Sending…");
    CHECK_STREQ(transfer_live_message(false, 0.0), "Receiving…");
}
