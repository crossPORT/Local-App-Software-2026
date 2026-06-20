#include "test_util.h"

namespace fabric_test {

std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

static int g_failures = 0;

int& failure_count() {
    return g_failures;
}

int register_test(const std::string& name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
    return 0;
}

int run_all(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    int total = 0;
    int passed = 0;
    int failed = 0;

    for (auto& tc : registry()) {
        if (!filter.empty() && tc.name.find(filter) == std::string::npos) {
            continue;
        }
        ++total;
        g_failures = 0;
        try {
            tc.fn();
        } catch (const std::exception& e) {
            ++g_failures;
            std::cerr << "    threw std::exception: " << e.what() << "\n";
        } catch (...) {
            ++g_failures;
            std::cerr << "    threw unknown exception\n";
        }
        if (g_failures == 0) {
            ++passed;
            std::cout << "[PASS] " << tc.name << "\n";
        } else {
            ++failed;
            std::cout << "[FAIL] " << tc.name << " (" << g_failures
                      << " failed checks)\n";
        }
    }

    std::cout << "\n"
              << passed << "/" << total << " tests passed";
    if (failed != 0) {
        std::cout << ", " << failed << " FAILED";
    }
    std::cout << std::endl;
    return failed == 0 ? 0 : 1;
}

}  // namespace fabric_test
