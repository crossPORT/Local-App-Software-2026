#pragma once

// Tiny dependency-free test framework. One executable per suite links this plus
// its test .cpp files and a thin main that calls fabric_test::run_all().
//
// Usage:
//   FABRIC_TEST(name_of_test) {
//       CHECK(cond);
//       CHECK_EQ(actual, expected);     // streamable types
//       CHECK_STREQ(a, b);              // std::string / const char*
//   }

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace fabric_test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry();
int& failure_count();  // failed checks within the current test
int register_test(const std::string& name, std::function<void()> fn);

// Runs all registered tests (optionally filtered by argv[1] substring).
// Returns 0 when every test passes, 1 otherwise.
int run_all(int argc, char** argv);

}  // namespace fabric_test

#define FABRIC_TEST(name)                                                      \
    static void name();                                                        \
    static const int fabric_reg_##name =                                       \
        ::fabric_test::register_test(#name, name);                             \
    static void name()

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ++::fabric_test::failure_count();                                  \
            std::cerr << "    CHECK failed: " #cond " ("                       \
                      << __FILE__ << ":" << __LINE__ << ")\n";                 \
        }                                                                      \
    } while (0)

#define CHECK_EQ(actual, expected)                                            \
    do {                                                                       \
        auto _va = (actual);                                                   \
        auto _vb = (expected);                                                 \
        if (!(_va == _vb)) {                                                   \
            ++::fabric_test::failure_count();                                  \
            std::cerr << "    CHECK_EQ failed: " #actual " == " #expected      \
                      << " [" << _va << " != " << _vb << "] ("                 \
                      << __FILE__ << ":" << __LINE__ << ")\n";                 \
        }                                                                      \
    } while (0)

#define CHECK_STREQ(actual, expected)                                         \
    do {                                                                       \
        const std::string _sa = (actual);                                      \
        const std::string _sb = (expected);                                    \
        if (_sa != _sb) {                                                      \
            ++::fabric_test::failure_count();                                  \
            std::cerr << "    CHECK_STREQ failed: " #actual " == " #expected   \
                      << " [\"" << _sa << "\" != \"" << _sb << "\"] ("         \
                      << __FILE__ << ":" << __LINE__ << ")\n";                 \
        }                                                                      \
    } while (0)
