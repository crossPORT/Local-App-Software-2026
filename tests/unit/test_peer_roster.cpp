#include "test_util.h"

#include "peer_roster.h"

#include <thread>

namespace {

PeerConfig make_peer(const std::string& name, const std::string& team,
                     ReceiveStatus status, int port) {
    PeerConfig p;
    p.display_name = name;
    p.team = team;
    p.receive_status = status;
    p.port_index = port;
    return p;
}

}  // namespace

FABRIC_TEST(roster_seed_and_find) {
    PeerRoster roster;
    roster.seed_from_config({
        make_peer("Creative-Desk", "Creative", ReceiveStatus::Open, 1),
        make_peer("CAD-Workstation", "CAD", ReceiveStatus::AskFirst, 0),
    });

    CHECK_EQ(roster.peers().size(), static_cast<size_t>(2));

    auto byName = roster.find_by_name("Creative-Desk");
    CHECK(byName.has_value());
    if (byName) {
        CHECK_EQ(byName->port_index, 1);
        CHECK(byName->receive_status == ReceiveStatus::Open);
        CHECK(!byName->online);
    }

    auto byPort = roster.find_by_port(0);
    CHECK(byPort.has_value());
    if (byPort) {
        CHECK_STREQ(byPort->display_name, "CAD-Workstation");
    }

    CHECK(!roster.find_by_name("Nobody").has_value());
}

FABRIC_TEST(roster_seed_skips_unnamed) {
    PeerRoster roster;
    roster.seed_from_config({make_peer("", "X", ReceiveStatus::Open, 2)});
    CHECK_EQ(roster.peers().size(), static_cast<size_t>(0));
}

FABRIC_TEST(roster_touch_updates_existing) {
    PeerRoster roster;
    roster.seed_from_config(
        {make_peer("Creative-Desk", "Creative", ReceiveStatus::Open, 1)});

    roster.touch_peer("Creative-Desk", "NewTeam", ReceiveStatus::Busy, 1);
    CHECK_EQ(roster.peers().size(), static_cast<size_t>(1));
    auto p = roster.find_by_name("Creative-Desk");
    CHECK(p.has_value());
    if (p) {
        CHECK_STREQ(p->team, "NewTeam");
        CHECK(p->receive_status == ReceiveStatus::Busy);
    }
}

FABRIC_TEST(roster_touch_adds_new) {
    PeerRoster roster;
    roster.touch_peer("Walk-Up", "Guest", ReceiveStatus::Open, 3);
    auto p = roster.find_by_port(3);
    CHECK(p.has_value());
    if (p) {
        CHECK_STREQ(p->display_name, "Walk-Up");
    }
}

FABRIC_TEST(roster_touch_same_port_different_name_updates_entry) {
    PeerRoster roster;
    roster.seed_from_config(
        {make_peer("CAD-Workstation", "CAD", ReceiveStatus::AskFirst, 0)});

    roster.touch_peer("Alice", "CAD", ReceiveStatus::Open, 0);
    CHECK_EQ(roster.peers().size(), static_cast<size_t>(1));

    auto byPort = roster.find_by_port(0);
    CHECK(byPort.has_value());
    if (byPort) {
        CHECK_STREQ(byPort->display_name, "Alice");
        CHECK(byPort->online);
        CHECK(byPort->receive_status == ReceiveStatus::Open);
    }

    CHECK(!roster.find_by_name("CAD-Workstation").has_value());
    CHECK(roster.find_by_name("Alice").has_value());
}

FABRIC_TEST(roster_touch_updates_port_on_name_match) {
    PeerRoster roster;
    roster.seed_from_config(
        {make_peer("Creative-Desk", "Creative", ReceiveStatus::Open, 1)});

    roster.touch_peer("Creative-Desk", "Creative", ReceiveStatus::Busy, 2);
    auto p = roster.find_by_name("Creative-Desk");
    CHECK(p.has_value());
    if (p) {
        CHECK_EQ(p->port_index, 2);
        CHECK(p->receive_status == ReceiveStatus::Busy);
    }
}

FABRIC_TEST(roster_set_online) {
    PeerRoster roster;
    roster.seed_from_config(
        {make_peer("Creative-Desk", "Creative", ReceiveStatus::Open, 1)});
    roster.set_peer_online(1, false);
    auto p = roster.find_by_port(1);
    CHECK(p.has_value());
    if (p) {
        CHECK(!p->online);
    }
}

FABRIC_TEST(roster_touch_presence_marks_online) {
    PeerRoster roster;
    roster.seed_from_config(
        {make_peer("CAD-Workstation", "CAD", ReceiveStatus::AskFirst, 0)});
    roster.touch_peer_presence("CAD-Workstation", "CAD");
    auto p = roster.find_by_name("CAD-Workstation");
    CHECK(p.has_value());
    if (p) {
        CHECK(p->online);
    }
}

FABRIC_TEST(roster_recent_peer_stays_online_within_window) {
    PeerRoster roster;
    roster.seed_from_config(
        {make_peer("Creative-Desk", "Creative", ReceiveStatus::Open, 1)});
    roster.touch_peer_presence("Creative-Desk", "Creative");
    // A generous window must not expire a peer we just heard from.
    roster.mark_stale_peers_offline(std::chrono::seconds(45));
    auto p = roster.find_by_name("Creative-Desk");
    CHECK(p.has_value());
    if (p) {
        CHECK(p->online);
    }
}

FABRIC_TEST(roster_set_all_peers_offline_clears_presence) {
    PeerRoster roster;
    roster.seed_from_config({
        make_peer("Creative-Desk", "Creative", ReceiveStatus::Open, 1),
        make_peer("CAD-Workstation", "CAD", ReceiveStatus::AskFirst, 0),
    });
    roster.touch_peer_presence("Creative-Desk", "Creative");
    roster.touch_peer_presence("CAD-Workstation", "CAD");
    roster.set_all_peers_offline();
    for (const auto& name : {"Creative-Desk", "CAD-Workstation"}) {
        auto p = roster.find_by_name(name);
        CHECK(p.has_value());
        if (p) {
            CHECK(!p->online);
        }
    }
}

FABRIC_TEST(roster_mark_stale_peers_offline) {
    PeerRoster roster;
    roster.seed_from_config(
        {make_peer("Creative-Desk", "Creative", ReceiveStatus::Open, 1)});
    roster.touch_peer_presence("Creative-Desk", "Creative");
    auto live = roster.find_by_name("Creative-Desk");
    CHECK(live.has_value());
    if (live) {
        CHECK(live->online);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    roster.mark_stale_peers_offline(std::chrono::seconds(0));
    auto stale = roster.find_by_name("Creative-Desk");
    CHECK(stale.has_value());
    if (stale) {
        CHECK(!stale->online);
    }
}
