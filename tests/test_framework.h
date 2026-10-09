// NOVA GPU - Software GPU
// tests/test_framework.h - minimal test registry + assertions.
//
// Deliberately tiny and dependency-free: tests must be runnable on any machine
// that can build the renderer, including a bare Windows console with no package
// manager available. Add new test files with NOVA_TEST(suite, name) and they are
// picked up automatically (static registration).
#pragma once

#include <cstdio>
#include <string>

namespace novatest {

using TestFunction = void (*)();

struct TestCase {
    const char* suite;
    const char* name;
    TestFunction function;
};

// One registry per binary, built on first use to dodge static init order issues.
void registerTest(const char* suite, const char* name, TestFunction function);

struct Registrar {
    Registrar(const char* suite, const char* name, TestFunction function) {
        registerTest(suite, name, function);
    }
};

// Assertion accounting; reset before each test case.
void resetAssertions();
int failureCount();
int checkCount();
// Increments and returns the assertion counter (used by the macros so that a
// naked CHECK(...) still counts as an executed check).
int countCheck();
void fail(const char* file, int line, const std::string& message);

// Formats a float for failure messages.
std::string toText(double value);

// Runs every registered test. Returns the number of failed tests.
int runAllTests();

}  // namespace novatest

#define NOVA_TEST(suite, name)                                                   \
    static void suite##_##name##_body();                                         \
    static ::novatest::Registrar nova_reg_##suite##_##name(#suite, #name,        \
                                                            &suite##_##name##_body); \
    static void suite##_##name##_body()

#define CHECK(condition)                                                          \
    do {                                                                          \
        (void)::novatest::countCheck();                                           \
        if (!(condition)) {                                                       \
            ::novatest::fail(__FILE__, __LINE__, std::string("CHECK failed: ") +  \
                                                     #condition);                 \
        }                                                                         \
    } while (false)

#define CHECK_EQ(actual, expected)                                                \
    do {                                                                          \
        (void)::novatest::countCheck();                                           \
        const auto nova_actual = (actual);                                        \
        const auto nova_expected = (expected);                                    \
        if (!(nova_actual == nova_expected)) {                                    \
            ::novatest::fail(__FILE__, __LINE__,                                  \
                             std::string("CHECK_EQ failed: " #actual " == "       \
                                         #expected " (actual=") +                 \
                                 ::novatest::toText(static_cast<double>(nova_actual)) + \
                                 ", expected=" +                                  \
                                 ::novatest::toText(static_cast<double>(nova_expected)) + ")"); \
        }                                                                         \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                   \
    do {                                                                          \
        (void)::novatest::countCheck();                                           \
        const double nova_actual = static_cast<double>(actual);                   \
        const double nova_expected = static_cast<double>(expected);               \
        const double nova_tolerance = static_cast<double>(tolerance);             \
        const double nova_delta = nova_actual - nova_expected;                    \
        const double nova_abs = nova_delta < 0 ? -nova_delta : nova_delta;        \
        if (!(nova_abs <= nova_tolerance)) {                                      \
            ::novatest::fail(__FILE__, __LINE__,                                  \
                             std::string("CHECK_NEAR failed: " #actual " ~= "     \
                                         #expected " (actual=") +                 \
                                 ::novatest::toText(nova_actual) + ", expected=" + \
                                 ::novatest::toText(nova_expected) + ", tolerance=" + \
                                 ::novatest::toText(nova_tolerance) + ")");       \
        }                                                                         \
    } while (false)

#define CHECK_TRUE(condition) CHECK(condition)
#define CHECK_FALSE(condition) CHECK(!(condition))
