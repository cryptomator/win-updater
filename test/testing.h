#pragma once

// Minimal test framework and helpers shared by the unit and integration tests.
#include "../src/handle.h"

#include <windows.h>

#include <string>
#include <vector>

struct Failure {
    std::string message;
};

// Fails the current test with file, line and condition.
#define CHECK(condition)                                                                                         \
    do {                                                                                                         \
        if (!(condition)) {                                                                                      \
            throw Failure{std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": CHECK(" #condition ")"};   \
        }                                                                                                        \
    } while (false)

struct TestCase {
    const char* name;
    void (*run)();
};

// Binaries under test and the directory for test files, set by the runner.
struct TestEnvironment {
    std::wstring updater;     // release build
    std::wstring testUpdater; // test build with stub verifier, `open` instead of `runas` and 3 s timeout
    std::wstring fake;        // test double, see fake.cpp
    std::wstring fixture;     // file signed by the real identity
    std::wstring tempRoot;    // long form, so that paths compare equal to the updater's final paths
};

extern TestEnvironment env;

// Skymatic's durable identity and the Microsoft root it chains up to, as in the signed fixture.
extern const wchar_t* const EKU;
extern const wchar_t* const ROOT;
extern const wchar_t* const ZEROS_32;

// ---- strings ----

std::string ToUtf8(const std::wstring& text);
bool Contains(const std::string& text, const std::string& part);

// ---- files ----

// Creates a new directory below env.tempRoot.
std::wstring NewDir(const std::wstring& name);
void Copy(const std::wstring& source, const std::wstring& target);
bool Exists(const std::wstring& path);
bool WaitForFile(const std::wstring& path, DWORD timeoutMs);
// Returns the file's content, or an empty string if it doesn't exist.
std::string ReadText(const std::wstring& path);
void WriteText(const std::wstring& path, const std::string& text);
// Hex digest as expected by --sha256. Fails if someone else has the file open for writing.
std::wstring Sha256Hex(const std::wstring& path);
bool CanOpenForWriting(const std::wstring& path);

// ---- processes ----

struct Child {
    UniqueHandle process;
    UniqueHandle out; // read end of the child's stdout, if redirected
    DWORD pid = 0;
};

// Starts a program with the given arguments, each quoted, optionally with stdout redirected into Child::out.
Child Start(const std::wstring& exe, const std::vector<std::wstring>& args, bool redirectStdout);
// Reads the child's stdout until it is closed, i.e. after READY or when the child exits.
std::string ReadStdout(Child& child);
// Waits for the child to exit and returns its exit code; fails the test on timeout.
DWORD WaitForExit(Child& child, DWORD timeoutMs);
void Kill(Child& child);
