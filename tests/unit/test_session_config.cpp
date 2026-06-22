#include "test_util.h"

#include "session_config.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

std::string write_conf(const std::string& body) {
    char tmpl[] = "/tmp/slsfabric-test-sessionconf-XXXXXX";
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

FABRIC_TEST(session_config_global_values) {
    const std::string path = write_conf(
        "source_dir=/src\n"
        "target_dir=/dst\n"
        "role=sender\n");

    SessionConfig cfg;
    CHECK(load_session_config_file(path, 0, cfg));
    CHECK_STREQ(cfg.source_dir, "/src");
    CHECK_STREQ(cfg.target_dir, "/dst");
    CHECK_STREQ(cfg.role, "sender");
    CHECK_STREQ(cfg.loaded_from, path);
    std::remove(path.c_str());
}

FABRIC_TEST(session_config_port_override) {
    const std::string path = write_conf(
        "role=sender\n"
        "[port0]\n"
        "role=receiver\n"
        "target_dir=/p0\n");

    SessionConfig p0;
    CHECK(load_session_config_file(path, 0, p0));
    CHECK_STREQ(p0.role, "receiver");
    CHECK_STREQ(p0.target_dir, "/p0");

    SessionConfig p1;
    CHECK(load_session_config_file(path, 1, p1));
    CHECK_STREQ(p1.role, "sender");  // falls back to global
    std::remove(path.c_str());
}

FABRIC_TEST(session_config_ignores_comments_and_blanks) {
    const std::string path = write_conf(
        "# a comment\n"
        "\n"
        "role=sender\n"
        "   # indented comment\n");

    SessionConfig cfg;
    CHECK(load_session_config_file(path, 0, cfg));
    CHECK_STREQ(cfg.role, "sender");
    std::remove(path.c_str());
}

FABRIC_TEST(session_config_missing_file_returns_false) {
    SessionConfig cfg;
    CHECK(!load_session_config_file("/tmp/does-not-exist-slsfabric.conf", 0, cfg));
}
