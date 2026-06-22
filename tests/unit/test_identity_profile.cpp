#include "test_util.h"

#include "identity_profile.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

// Writes a config to a unique temp path and returns it.
std::string write_conf(const std::string& body) {
    char tmpl[] = "/tmp/slsfabric-test-conf-XXXXXX";
    const int fd = mkstemp(tmpl);
    if (fd < 0) {
        return {};
    }
    std::ofstream out(tmpl, std::ios::trunc);
    out << body;
    out.close();
    ::close(fd);
    return tmpl;
}

}  // namespace

FABRIC_TEST(identity_basic_fields) {
    const std::string path = write_conf(
        "display_name=CAD-Workstation\n"
        "team=CAD\n"
        "receive_status=ask_first\n"
        "receive_folder=/tmp/inbox-cad\n");

    IdentityProfile p;
    CHECK(load_identity_profile(0, path, p));
    CHECK_STREQ(p.display_name, "CAD-Workstation");
    CHECK_STREQ(p.team, "CAD");
    CHECK(p.receive_status == ReceiveStatus::AskFirst);
    CHECK_STREQ(p.receive_folder, "/tmp/inbox-cad");
    CHECK_EQ(p.transfer_timeout_ms, 0);
    CHECK_EQ(p.usb_inflight_mb, 0);
    std::remove(path.c_str());
}

FABRIC_TEST(identity_tuning_keys_parsed) {
    const std::string path = write_conf(
        "display_name=Node\n"
        "transfer_timeout_ms=5000\n"
        "usb_inflight_mb=64\n"
        "booth_display_mib_s=7168\n"
        "booth_display_jitter_pct=3\n");

    IdentityProfile p;
    CHECK(load_identity_profile(0, path, p));
    CHECK_EQ(p.transfer_timeout_ms, 5000);
    CHECK_EQ(p.usb_inflight_mb, 64);
    CHECK(p.booth_display_mib_s > 7167.0 && p.booth_display_mib_s < 7169.0);
    CHECK(p.booth_display_jitter_pct > 2.9 && p.booth_display_jitter_pct < 3.1);
    std::remove(path.c_str());
}

FABRIC_TEST(identity_bad_tuning_values_fall_back_to_zero) {
    const std::string path = write_conf(
        "display_name=Node\n"
        "transfer_timeout_ms=notanumber\n"
        "usb_inflight_mb=\n");

    IdentityProfile p;
    CHECK(load_identity_profile(0, path, p));
    CHECK_EQ(p.transfer_timeout_ms, 0);
    CHECK_EQ(p.usb_inflight_mb, 0);
    std::remove(path.c_str());
}

FABRIC_TEST(identity_port_section_overrides_global) {
    const std::string path = write_conf(
        "display_name=Global\n"
        "transfer_timeout_ms=1000\n"
        "[port1]\n"
        "display_name=PortOne\n"
        "transfer_timeout_ms=9000\n");

    IdentityProfile p0;
    CHECK(load_identity_profile(0, path, p0));
    CHECK_STREQ(p0.display_name, "Global");
    CHECK_EQ(p0.transfer_timeout_ms, 1000);

    IdentityProfile p1;
    CHECK(load_identity_profile(1, path, p1));
    CHECK_STREQ(p1.display_name, "PortOne");
    CHECK_EQ(p1.transfer_timeout_ms, 9000);
    std::remove(path.c_str());
}

FABRIC_TEST(identity_peers_loaded) {
    const std::string path = write_conf(
        "display_name=CAD-Workstation\n"
        "team=CAD\n"
        "[peer0]\n"
        "display_name=Creative-Desk\n"
        "team=Creative\n"
        "receive_status=open\n"
        "port_index=1\n");

    IdentityProfile p;
    CHECK(load_identity_profile(0, path, p));
    CHECK_EQ(p.peers.size(), static_cast<size_t>(1));
    if (!p.peers.empty()) {
        CHECK_STREQ(p.peers[0].display_name, "Creative-Desk");
        CHECK_STREQ(p.peers[0].team, "Creative");
        CHECK(p.peers[0].receive_status == ReceiveStatus::Open);
        CHECK_EQ(p.peers[0].port_index, 1);
    }
    std::remove(path.c_str());
}

FABRIC_TEST(identity_default_receive_folder) {
    const std::string path = write_conf("display_name=NoFolder\n");
    IdentityProfile p;
    CHECK(load_identity_profile(0, path, p));
    CHECK(!p.receive_folder.empty());  // defaults to ~/Incoming
    std::remove(path.c_str());
}

FABRIC_TEST(receive_status_string_round_trip) {
    CHECK(receive_status_from_string("open") == ReceiveStatus::Open);
    CHECK(receive_status_from_string("ask_first") == ReceiveStatus::AskFirst);
    CHECK(receive_status_from_string("busy") == ReceiveStatus::Busy);
    CHECK(receive_status_from_string("???") == ReceiveStatus::AskFirst);
    CHECK_STREQ(receive_status_to_string(ReceiveStatus::Open), "open");
    CHECK_STREQ(receive_status_to_string(ReceiveStatus::Busy), "busy");
}
