// NOVA GPU - Software GPU
// tests/test_framework.cpp - the test runner (see test_framework.h).
#include "test_framework.h"

#include <cstring>
#include <vector>

namespace novatest {
namespace {

std::vector<TestCase>& tests() {
    static std::vector<TestCase> registry;
    return registry;
}

int g_failures = 0;
int g_checks = 0;

std::vector<std::string>& failureMessages() {
    static std::vector<std::string> messages;
    return messages;
}

}  // namespace

void registerTest(const char* suite, const char* name, TestFunction function) {
    tests().push_back(TestCase{suite, name, function});
}

void resetAssertions() {
    g_failures = 0;
    g_checks = 0;
    failureMessages().clear();
}

int failureCount() { return g_failures; }
int checkCount() { return g_checks; }
int countCheck() { return ++g_checks; }

void fail(const char* file, int line, const std::string& message) {
    ++g_failures;
    // Trim the directory part of the file name so failures stay readable.
    const char* shortFile = std::strrchr(file, '/');
    const char* shortFileWin = std::strrchr(file, '\\');
    if (shortFileWin != nullptr && (shortFile == nullptr || shortFileWin > shortFile)) {
        shortFile = shortFileWin;
    }
    if (shortFile != nullptr) ++shortFile;

    failureMessages().push_back(std::string(shortFile) + ":" + std::to_string(line) + ": " + message);
}

std::string toText(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.6g", value);
    return buffer;
}

int runAllTests() {
    int failedTests = 0;
    int passedTests = 0;
    int totalChecks = 0;

    int currentSuiteLength = 0;
    const char* currentSuite = nullptr;

    for (const TestCase& test : tests()) {
        // Print a header whenever the suite changes.
        if (currentSuite == nullptr || std::strcmp(currentSuite, test.suite) != 0) {
            currentSuite = test.suite;
            std::printf("\n[%s]\n", currentSuite);
            currentSuiteLength = static_cast<int>(std::strlen(currentSuite));
        }
        (void)currentSuiteLength;

        resetAssertions();
        test.function();
        const int checks = checkCount();
        const int failures = failureCount();
        totalChecks += checks;

        if (failures == 0) {
            ++passedTests;
            std::printf("  [ PASS ] %-44s (%d checks)\n", test.name, checks);
        } else {
            ++failedTests;
            std::printf("  [ FAIL ] %-44s (%d checks, %d failed)\n", test.name, checks, failures);
            for (const std::string& message : failureMessages()) {
                std::printf("           %s\n", message.c_str());
            }
        }
    }

    std::printf("\n%d tests: %d passed, %d failed (%d checks)\n",
                static_cast<int>(tests().size()), passedTests, failedTests, totalChecks);
    return failedTests;
}

}  // namespace novatest

int main() {
    const int failed = novatest::runAllTests();
    if (failed != 0) {
        std::printf("RESULT: FAILED (%d test(s) failed)\n", failed);
        return 1;
    }
    std::printf("RESULT: OK\n");
    return 0;
}
