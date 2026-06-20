#include "test_util.h"

#include "demo_config.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

std::string write_conf(const std::string& body) {
    char tmpl[] = "/tmp/slsfabric-test-democonf-XXXXXX";
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

FABRIC_TEST(demo_config_global_values) {
    const std::string path = write_conf(
        "source_dir=/src\n"
        "target_dir=/dst\n"
        "role=sender\n");

    DemoConfig cfg;
    CHECK(load_demo_config_file(path, 0, cfg));
    CHECK_STREQ(cfg.source_dir, "/src");
    CHECK_STREQ(cfg.target_dir, "/dst");
    CHECK_STREQ(cfg.role, "sender");
    CHECK_STREQ(cfg.loaded_from, path);
    std::remove(path.c_str());
}

FABRIC_TEST(demo_config_port_override) {
    const std::string path = write_conf(
        "role=sender\n"
        "[port0]\n"
        "role=receiver\n"
        "target_dir=/p0\n");

    DemoConfig p0;
    CHECK(load_demo_config_file(path, 0, p0));
    CHECK_STREQ(p0.role, "receiver");
    CHECK_STREQ(p0.target_dir, "/p0");

    DemoConfig p1;
    CHECK(load_demo_config_file(path, 1, p1));
    CHECK_STREQ(p1.role, "sender");  // falls back to global
    std::remove(path.c_str());
}

FABRIC_TEST(demo_config_ignores_comments_and_blanks) {
    const std::string path = write_conf(
        "# a comment\n"
        "\n"
        "role=sender\n"
        "   # indented comment\n");

    DemoConfig cfg;
    CHECK(load_demo_config_file(path, 0, cfg));
    CHECK_STREQ(cfg.role, "sender");
    std::remove(path.c_str());
}

FABRIC_TEST(demo_config_missing_file_returns_false) {
    DemoConfig cfg;
    CHECK(!load_demo_config_file("/tmp/does-not-exist-slsfabric.conf", 0, cfg));
}
