// Test runner for the unit tests (unit_tests.cpp) and integration tests (integration_tests.cpp).
// Usage: tests.exe <release updater> <test build of the updater> <fake.exe> <signed fixture>
#include "testing.h"

#include <cstdio>

std::vector<TestCase> UnitTests();
std::vector<TestCase> IntegrationTests();

namespace {

    std::wstring FullPath(const wchar_t* path) {
        wchar_t buffer[MAX_PATH];
        DWORD length = GetFullPathNameW(path, MAX_PATH, buffer, nullptr);
        return std::wstring(buffer, length);
    }

    // Long form of %TEMP% (it may contain 8.3 names), without trailing backslash.
    std::wstring LongTempPath() {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        wchar_t longTemp[MAX_PATH];
        DWORD length = GetLongPathNameW(temp, longTemp, MAX_PATH);
        std::wstring path(longTemp, length);
        if (!path.empty() && path.back() == L'\\') {
            path.pop_back();
        }
        return path;
    }

    // Runs the tests and returns the number of failures.
    int RunAll(const char* suite, const std::vector<TestCase>& tests) {
        std::printf("-- %s\n", suite);
        int failures = 0;
        for (const TestCase& test : tests) {
            try {
                test.run();
                std::printf("PASS %s\n", test.name);
            } catch (const Failure& failure) {
                ++failures;
                std::printf("FAIL %s: %s\n", test.name, failure.message.c_str());
            }
        }
        return failures;
    }

}

int wmain(int argc, wchar_t** argv) {
    if (argc != 5) {
        std::fwprintf(stderr, L"Usage: tests.exe <release updater> <test build of the updater> <fake.exe> <signed fixture>\n");
        return 2;
    }
    env.updater = FullPath(argv[1]);
    env.testUpdater = FullPath(argv[2]);
    env.fake = FullPath(argv[3]);
    env.fixture = FullPath(argv[4]);
    env.tempRoot = LongTempPath() + L"\\win-updater-tests-" + std::to_wstring(GetCurrentProcessId());
    if (!CreateDirectoryW(env.tempRoot.c_str(), nullptr)) {
        std::fwprintf(stderr, L"Failed to create %ls\n", env.tempRoot.c_str());
        return 2;
    }

    std::vector<TestCase> unitTests = UnitTests();
    std::vector<TestCase> integrationTests = IntegrationTests();
    int failures = RunAll("unit tests", unitTests);
    failures += RunAll("integration tests", integrationTests);
    std::printf("Tests run: %zu, failures: %d, work dir: %s\n", unitTests.size() + integrationTests.size(), failures, ToUtf8(env.tempRoot).c_str());
    return failures == 0 ? 0 : 1;
}
