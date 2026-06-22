#include "test_util.h"

#include "integration/orchestrator_harness.h"
#include "receive_payload.h"

#include <fstream>
#include <string>

namespace {

constexpr auto kWait = std::chrono::seconds(30);

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

FABRIC_TEST(integration_announce_peer_visible) {
    integration::SimFabricScope sim;
    const std::string alice_dir = integration::make_temp_dir("slsfabric-alice-");
    const std::string bob_dir = integration::make_temp_dir("slsfabric-bob-");
    CHECK(!alice_dir.empty());
    CHECK(!bob_dir.empty());

    integration::TrackedOrchestrator alice(
        0, integration::make_identity("Alice", alice_dir, ReceiveStatus::Open));
    integration::TrackedOrchestrator bob(
        1, integration::make_identity("Bob", bob_dir, ReceiveStatus::Open));
    alice.start();
    bob.start();

    CHECK(bob.wait_for_peer_online("Alice", kWait));
    CHECK(alice.wait_for_peer_online("Bob", kWait));

    alice.stop();
    bob.stop();
}

FABRIC_TEST(integration_handshake_open_auto_accept) {
    integration::SimFabricScope sim;
    const std::string alice_dir = integration::make_temp_dir("slsfabric-alice-");
    const std::string bob_dir = integration::make_temp_dir("slsfabric-bob-");
    CHECK(!alice_dir.empty());
    CHECK(!bob_dir.empty());

    const std::string payload(8192, 'H');
    const std::string src = integration::write_temp_file(payload);
    CHECK(!src.empty());

    integration::TrackedOrchestrator alice(
        0, integration::make_identity("Alice", alice_dir, ReceiveStatus::Open));
    integration::TrackedOrchestrator bob(
        1, integration::make_identity("Bob", bob_dir, ReceiveStatus::Open));
    alice.start();
    bob.start();

    CHECK(bob.wait_for_peer_online("Alice", kWait));
    CHECK(bob.orchestrator().send_to_peer("Alice", {src}, "integration test"));

    CHECK(alice.wait_for_notification_substring("Received", kWait));
    CHECK(bob.wait_for_notification_substring("Sent", kWait));

    const std::string basename = src.substr(src.find_last_of('/') + 1);
    const std::string actual_path = build_inbound_payload_path(alice_dir, basename);
    CHECK(read_file(actual_path) == payload);

    alice.stop();
    bob.stop();
    std::remove(src.c_str());
}

FABRIC_TEST(integration_handshake_wx_sender_alice_to_bob) {
    integration::SimFabricScope sim;
    const std::string alice_dir = integration::make_temp_dir("slsfabric-alice-");
    const std::string bob_dir = integration::make_temp_dir("slsfabric-bob-");
    CHECK(!alice_dir.empty());
    CHECK(!bob_dir.empty());

    const std::string payload(8192, 'A');
    const std::string src = integration::write_temp_file(payload);
    CHECK(!src.empty());

    integration::TrackedOrchestrator alice(
        0, integration::make_identity("Alice", alice_dir, ReceiveStatus::Open));
    integration::TrackedOrchestrator bob(
        1, integration::make_identity("Bob", bob_dir, ReceiveStatus::Open));
    alice.start();
    bob.start();

    CHECK(alice.wait_for_peer_online("Bob", kWait));
    CHECK(alice.orchestrator().send_to_peer("Bob", {src}, "alice sends to bob"));

    CHECK(bob.wait_for_notification_substring("Received", kWait));
    CHECK(alice.wait_for_notification_substring("Sent", kWait));

    const std::string basename = src.substr(src.find_last_of('/') + 1);
    const std::string actual_path = build_inbound_payload_path(bob_dir, basename);
    CHECK(read_file(actual_path) == payload);

    alice.stop();
    bob.stop();
    std::remove(src.c_str());
}

FABRIC_TEST(integration_handshake_bidirectional) {
    integration::SimFabricScope sim;
    const std::string alice_dir = integration::make_temp_dir("slsfabric-alice-");
    const std::string bob_dir = integration::make_temp_dir("slsfabric-bob-");
    CHECK(!alice_dir.empty());
    CHECK(!bob_dir.empty());

    const std::string payload_a(4096, 'A');
    const std::string payload_b(4096, 'B');
    const std::string src_a = integration::write_temp_file(payload_a);
    const std::string src_b = integration::write_temp_file(payload_b);
    CHECK(!src_a.empty());
    CHECK(!src_b.empty());

    integration::TrackedOrchestrator alice(
        0, integration::make_identity("Alice", alice_dir, ReceiveStatus::Open));
    integration::TrackedOrchestrator bob(
        1, integration::make_identity("Bob", bob_dir, ReceiveStatus::Open));
    alice.start();
    bob.start();

    CHECK(alice.wait_for_peer_online("Bob", kWait));

    // Alice -> Bob (wx/PWA sender direction covered elsewhere).
    CHECK(alice.orchestrator().send_to_peer("Bob", {src_a}, "alice first"));
    CHECK(bob.wait_for_notification_substring("Received", kWait));
    CHECK(alice.wait_for_notification_substring("Sent", kWait));

    const std::string bob_got_a =
        build_inbound_payload_path(bob_dir, src_a.substr(src_a.find_last_of('/') + 1));
    CHECK(read_file(bob_got_a) == payload_a);

    // Bob -> Alice immediately after (matches real booth: reverse direction).
    CHECK(bob.wait_for_peer_online("Alice", kWait));
    CHECK(bob.orchestrator().send_to_peer("Alice", {src_b}, "bob second"));
    CHECK(alice.wait_for_notification_substring("Received", kWait));
    CHECK(bob.wait_for_notification_substring("Sent", kWait));

    const std::string alice_got_b =
        build_inbound_payload_path(alice_dir, src_b.substr(src_b.find_last_of('/') + 1));
    CHECK(read_file(alice_got_b) == payload_b);

    alice.stop();
    bob.stop();
    std::remove(src_a.c_str());
    std::remove(src_b.c_str());
}

FABRIC_TEST(integration_handshake_decline) {
    integration::SimFabricScope sim;
    const std::string alice_dir = integration::make_temp_dir("slsfabric-alice-");
    const std::string bob_dir = integration::make_temp_dir("slsfabric-bob-");
    CHECK(!alice_dir.empty());
    CHECK(!bob_dir.empty());

    const std::string payload(1024, 'D');
    const std::string src = integration::write_temp_file(payload);
    CHECK(!src.empty());

    integration::TrackedOrchestrator alice(
        0, integration::make_identity("Alice", alice_dir, ReceiveStatus::AskFirst));
    integration::TrackedOrchestrator bob(
        1, integration::make_identity("Bob", bob_dir, ReceiveStatus::Open));
    alice.start();
    bob.start();

    CHECK(bob.wait_for_peer_online("Alice", kWait));
    CHECK(bob.orchestrator().send_to_peer("Alice", {src}, "decline test"));

    CHECK(alice.wait_until(
        [](const OrchestratorUiState& state) { return static_cast<bool>(state.pending_offer); },
        kWait));

    alice.orchestrator().decline_pending_offer();

    CHECK(bob.wait_until(
        [](const OrchestratorUiState& state) {
            return !state.busy && state.error_message == "Declined";
        },
        kWait));

    alice.stop();
    bob.stop();
    std::remove(src.c_str());
}
