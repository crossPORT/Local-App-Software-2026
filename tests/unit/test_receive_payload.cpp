#include "test_util.h"

#include "receive_payload.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

std::string temp_dir() {
    return (std::filesystem::temp_directory_path() / "slsfabric-receive-payload-test")
        .string();
}

std::string write_bytes(const std::string& path, std::size_t nbytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    std::vector<char> buf(4096, 'x');
    std::size_t written = 0;
    while (written < nbytes) {
        const std::size_t n = std::min(buf.size(), nbytes - written);
        out.write(buf.data(), static_cast<std::streamsize>(n));
        written += n;
    }
    out.close();
    return path;
}

TransferResult failed_usb(const std::string& err) {
    return TransferResult{false, 0, 100, 0.0, 0.0, err};
}

TransferResult ok_usb(uint64_t bytes) {
    return TransferResult{true, bytes, bytes, 1.0, 1.0, {}};
}

}  // namespace

FABRIC_TEST(inbound_payload_path_builds_under_receive_folder) {
    CHECK_STREQ(build_inbound_payload_path("/inbox", "model.stl"), "/inbox/model.stl");
    CHECK_STREQ(build_inbound_payload_path("/inbox/", "model.stl"), "/inbox/model.stl");
    CHECK_STREQ(build_inbound_payload_path("/inbox", ""), "/inbox/incoming.bin");
}

FABRIC_TEST(should_remove_partial_only_on_failed_receive) {
    CHECK(should_remove_partial_after_receive(failed_usb("USB receive failed, status=2")));
    CHECK(!should_remove_partial_after_receive(ok_usb(1024)));
}

FABRIC_TEST(cleanup_missing_file_is_noop) {
    const std::string path = temp_dir() + "/does-not-exist.bin";
    std::filesystem::remove_all(temp_dir());

    const PartialReceiveCleanup r = cleanup_partial_receive_file(path);
    CHECK(!r.existed_before);
    CHECK(!r.removed);
    CHECK(r.path_clear);
    CHECK(!std::filesystem::exists(path));
}

FABRIC_TEST(cleanup_removes_empty_partial_file) {
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::string path = temp_dir() + "/empty.bin";
    write_bytes(path, 0);

    const PartialReceiveCleanup r = cleanup_partial_receive_file(path);
    CHECK(r.existed_before);
    CHECK_EQ(r.bytes_removed, 0ull);
    CHECK(r.removed);
    CHECK(r.path_clear);
    CHECK(!std::filesystem::exists(path));

    std::filesystem::remove_all(temp_dir());
}

FABRIC_TEST(cleanup_removes_nonempty_partial_file) {
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::string path = temp_dir() + "/partial.bin";
    constexpr std::size_t kPartial = 4194304;  // 4 MiB — one chunk
    write_bytes(path, kPartial);

    const PartialReceiveCleanup r = cleanup_partial_receive_file(path);
    CHECK(r.existed_before);
    CHECK_EQ(r.bytes_removed, static_cast<uint64_t>(kPartial));
    CHECK(r.removed);
    CHECK(r.path_clear);
    CHECK(!std::filesystem::exists(path));

    std::filesystem::remove_all(temp_dir());
}

FABRIC_TEST(handle_failed_inbound_receive_cleans_and_preserves_error) {
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::string path = temp_dir() + "/aborted.bin";
    write_bytes(path, 8192);

    const FailedInboundReceive out =
        handle_failed_inbound_receive(failed_usb("USB receive failed, status=2"), path);
    CHECK_STREQ(out.error_message, "USB receive failed, status=2");
    CHECK(out.cleanup.removed);
    CHECK(out.cleanup.path_clear);
    CHECK(!std::filesystem::exists(path));

    std::filesystem::remove_all(temp_dir());
}

FABRIC_TEST(handle_failed_inbound_receive_skips_cleanup_on_success) {
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::string path = temp_dir() + "/complete.bin";
    write_bytes(path, 512);

    const FailedInboundReceive out = handle_failed_inbound_receive(ok_usb(512), path);
    CHECK(out.error_message.empty());
    CHECK(!out.cleanup.removed);
    CHECK(out.cleanup.path_clear);
    CHECK(std::filesystem::exists(path));

    std::filesystem::remove_all(temp_dir());
}
