#include "test_util.h"

#include "fabric_tar_pack.h"
#include "platform_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string unique_test_dir(const char* prefix) {
    return (std::filesystem::temp_directory_path()
            / (std::string(prefix) + "-"
               + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
        .string();
}

bool command_available(const char* name) {
    std::string error;
    return platform::run_command({name, "--version"}, &error);
}

std::string write_fixture(const std::filesystem::path& dir,
                          const std::string& name,
                          const std::string& content) {
    const std::filesystem::path path = dir / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << content;
    return path.string();
}

}  // namespace

FABRIC_TEST(tar_pack_multiple_files) {
    if (!command_available("tar")) {
        std::cerr << "    skipped: tar not available\n";
        return;
    }

    const std::filesystem::path staging = unique_test_dir("slsfabric-tar-test");
    std::error_code ec;
    std::filesystem::create_directories(staging, ec);
    CHECK(!ec);

    const std::string file_a = write_fixture(staging, "alpha.txt", "alpha");
    const std::string file_b = write_fixture(staging, "beta.txt", "beta");

    std::string pack_error;
    const std::string tar_path =
        create_tar_for_paths({file_a, file_b}, &pack_error);
    CHECK(!tar_path.empty());

    const std::filesystem::path extract_dir = staging / "out";
    std::filesystem::create_directories(extract_dir, ec);
    CHECK(!ec);

    std::string extract_error;
    CHECK(extract_tar_to_dir(tar_path, extract_dir.string(), &extract_error));
    CHECK(std::filesystem::is_regular_file(extract_dir / "alpha.txt"));
    CHECK(std::filesystem::is_regular_file(extract_dir / "beta.txt"));

    std::remove(tar_path.c_str());
    std::filesystem::remove_all(staging, ec);
}

FABRIC_TEST(tar_pack_disambiguates_same_basename) {
    if (!command_available("tar")) {
        std::cerr << "    skipped: tar not available\n";
        return;
    }

    const std::filesystem::path staging = unique_test_dir("slsfabric-tar-dup");
    std::error_code ec;
    std::filesystem::create_directories(staging, ec);
    CHECK(!ec);

    const std::filesystem::path dir_a = staging / "dir_a";
    const std::filesystem::path dir_b = staging / "dir_b";
    std::filesystem::create_directories(dir_a, ec);
    std::filesystem::create_directories(dir_b, ec);
    CHECK(!ec);

    const std::string file_a = write_fixture(dir_a, "same.txt", "from-a");
    const std::string file_b = write_fixture(dir_b, "same.txt", "from-b");

    std::string pack_error;
    const std::string tar_path = create_tar_for_paths({file_a, file_b}, &pack_error);
    CHECK(!tar_path.empty());

    const std::filesystem::path extract_dir = staging / "out";
    std::filesystem::create_directories(extract_dir, ec);
    CHECK(!ec);

    std::string extract_error;
    CHECK(extract_tar_to_dir(tar_path, extract_dir.string(), &extract_error));
    CHECK(std::filesystem::is_regular_file(extract_dir / "dir_a" / "same.txt"));
    CHECK(std::filesystem::is_regular_file(extract_dir / "dir_b" / "same.txt"));

    std::remove(tar_path.c_str());
    std::filesystem::remove_all(staging, ec);
}
