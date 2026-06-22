#include "test_util.h"

#include "fabric_session_message.h"

#include <fstream>
#include <sstream>
#include <string>

#ifndef FABRIC_TEST_FIXTURE_DIR
#define FABRIC_TEST_FIXTURE_DIR "tests/fixtures"
#endif

namespace {

std::string fixture_path(const std::string& name) {
    std::ostringstream out;
    out << FABRIC_TEST_FIXTURE_DIR << "/session/" << name;
    return out.str();
}

}  // namespace

FABRIC_TEST(session_fixture_offer_round_trip) {
    const std::string path = fixture_path("offer.sample.msg");
    FabricSessionMessage got{};
    CHECK(read_session_file(path, got));
    CHECK(got.kind == SessionMessageKind::Offer);
    CHECK_STREQ(got.from_name, "Bob");
    CHECK_STREQ(got.team, "Test");
    CHECK_STREQ(got.session_id, "golden-offer-01");
    CHECK_STREQ(got.to_name, "Alice");
    CHECK_STREQ(got.note, "fixture");
    CHECK_STREQ(got.payload_type, "file");
    CHECK_STREQ(got.payload_name, "handshake.bin");
    CHECK_EQ(got.file_count, 1u);
    CHECK_EQ(got.total_bytes, 4096u);
}
