#include "test_util.h"

#include "fabric_meta_file.h"

#include <cstdio>
#include <filesystem>
#include <string>

FABRIC_TEST(meta_write_read_round_trip) {
    FabricSendMeta meta;
    meta.type = "file";
    meta.relative_name = "subdir/model.stl";

    const std::string path = write_meta_temp_file(meta);
    CHECK(!path.empty());

    FabricSendMeta got;
    CHECK(read_meta_file(path, got));
    CHECK_STREQ(got.type, "file");
    CHECK_STREQ(got.relative_name, "subdir/model.stl");
    std::remove(path.c_str());
}

FABRIC_TEST(meta_type_predicates) {
    FabricSendMeta f;
    f.type = "file";
    CHECK(fabric_meta_is_file(f));
    CHECK(!fabric_meta_is_tar_archive(f));

    FabricSendMeta t;
    t.type = "tar";
    CHECK(fabric_meta_is_tar_archive(t));
    CHECK(!fabric_meta_is_file(t));
}

FABRIC_TEST(meta_write_rejects_unsafe_names) {
    for (const char* bad : {"", ".", "..", "../escape", "a/../../etc/passwd"}) {
        FabricSendMeta meta;
        meta.relative_name = bad;
        CHECK(write_meta_temp_file(meta).empty());
    }
}

FABRIC_TEST(meta_target_path_blocks_traversal) {
    // Leading slash is stripped, traversal is rejected entirely.
    CHECK(target_path_for_name("/inbox", "../../etc/passwd").empty());
    CHECK(target_path_for_name("/inbox", "..").empty());
    CHECK(target_path_for_name("", "ok.bin").empty());

    CHECK_STREQ(target_path_for_name("/inbox", "ok.bin"), "/inbox/ok.bin");
    // Trailing slash on dir is normalized to a single separator.
    CHECK_STREQ(target_path_for_name("/inbox/", "a/b.bin"), "/inbox/a/b.bin");
    // Absolute relative name is de-rooted onto the target dir.
    CHECK_STREQ(target_path_for_name("/inbox", "/abs.bin"), "/inbox/abs.bin");
}

FABRIC_TEST(meta_ensure_parent_directories) {
    const std::string base =
        (std::filesystem::temp_directory_path() / "slsfabric-test-mkdir").string();
    std::filesystem::remove_all(base);
    const std::string file = base + "/a/b/c/file.bin";

    std::string err;
    CHECK(ensure_parent_directories(file, &err));
    CHECK(std::filesystem::is_directory(base + "/a/b/c"));

    std::filesystem::remove_all(base);
}
