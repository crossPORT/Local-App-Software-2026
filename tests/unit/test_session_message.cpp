#include "test_util.h"

#include "fabric_session_message.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

std::string write_raw(const std::string& contents) {
    char tmpl[] = "/tmp/slsfabric-test-sess-XXXXXX";
    const int fd = mkstemp(tmpl);
    if (fd < 0) {
        return {};
    }
    std::ofstream out(tmpl, std::ios::binary | std::ios::trunc);
    out << contents;
    out.close();
    ::close(fd);
    return tmpl;
}

}  // namespace

FABRIC_TEST(session_kind_string_round_trip) {
    const SessionMessageKind kinds[] = {
        SessionMessageKind::Offer,   SessionMessageKind::Accept,
        SessionMessageKind::Decline, SessionMessageKind::Ready,
        SessionMessageKind::Announce};
    for (auto k : kinds) {
        CHECK(session_kind_from_string(session_kind_to_string(k)) == k);
    }
    CHECK(session_kind_from_string("garbage") == SessionMessageKind::Unknown);
    CHECK_STREQ(session_kind_to_string(SessionMessageKind::Unknown), "unknown");
}

FABRIC_TEST(session_offer_write_read_round_trip) {
    FabricSessionMessage msg;
    msg.kind = SessionMessageKind::Offer;
    msg.from_name = "CAD-Workstation";
    msg.team = "CAD";
    msg.session_id = "abc123";
    msg.to_name = "Creative-Desk";
    msg.note = "here you go";
    msg.payload_type = "file";
    msg.payload_name = "model.stl";
    msg.file_count = 1;
    msg.total_bytes = 123456789ull;

    const std::string path = write_session_temp_file(msg);
    CHECK(!path.empty());

    FabricSessionMessage got;
    CHECK(read_session_file(path, got));
    CHECK(got.kind == SessionMessageKind::Offer);
    CHECK_STREQ(got.from_name, "CAD-Workstation");
    CHECK_STREQ(got.team, "CAD");
    CHECK_STREQ(got.session_id, "abc123");
    CHECK_STREQ(got.to_name, "Creative-Desk");
    CHECK_STREQ(got.note, "here you go");
    CHECK_STREQ(got.payload_type, "file");
    CHECK_STREQ(got.payload_name, "model.stl");
    CHECK_EQ(got.file_count, 1u);
    CHECK_EQ(got.total_bytes, 123456789ull);

    std::remove(path.c_str());
}

FABRIC_TEST(session_write_requires_session_id) {
    FabricSessionMessage msg;
    msg.kind = SessionMessageKind::Announce;
    msg.from_name = "X";
    // no session_id
    CHECK(write_session_temp_file(msg).empty());
}

FABRIC_TEST(session_read_rejects_bad_header) {
    const std::string path = write_raw("NOT-A-FABRIC-HEADER\nkind=offer\nsession_id=z\n");
    CHECK(!path.empty());
    FabricSessionMessage got;
    CHECK(!read_session_file(path, got));
    std::remove(path.c_str());
}

FABRIC_TEST(session_read_rejects_unknown_kind) {
    const std::string path =
        write_raw("FABRIC-SESSION-v1\nkind=bogus\nsession_id=z\n");
    FabricSessionMessage got;
    CHECK(!read_session_file(path, got));
    std::remove(path.c_str());
}

FABRIC_TEST(session_read_rejects_missing_session_id) {
    const std::string path = write_raw("FABRIC-SESSION-v1\nkind=offer\nfrom=A\n");
    FabricSessionMessage got;
    CHECK(!read_session_file(path, got));
    std::remove(path.c_str());
}

FABRIC_TEST(session_read_tolerates_crlf_and_blank_lines) {
    // The header line is always LF (machine-generated); body lines may carry
    // CRLF and blank separators, which the reader strips/ignores.
    const std::string path = write_raw(
        "FABRIC-SESSION-v1\nkind=ready\r\n\r\nsession_id=s9\r\nfrom=Node\r\n");
    FabricSessionMessage got;
    CHECK(read_session_file(path, got));
    CHECK(got.kind == SessionMessageKind::Ready);
    CHECK_STREQ(got.session_id, "s9");
    CHECK_STREQ(got.from_name, "Node");
    std::remove(path.c_str());
}

FABRIC_TEST(session_inbox_path_helpers) {
    const std::string rel = session_inbox_relative_path("deadbeef");
    CHECK_STREQ(rel, ".fabric-session/deadbeef.msg");
    CHECK(is_session_inbox_path(rel));
    CHECK(!is_session_inbox_path("Incoming/file.bin"));
    CHECK(!is_session_inbox_path(".fabric-session/x.txt"));
}
