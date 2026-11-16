#include "testing.h"

#include "../src/verify.h"

#include <array>

TestEnvironment env;

const wchar_t* const EKU = L"1.3.6.1.4.1.311.97.986645502.209118791.449271898.355477593";
const wchar_t* const ROOT = L"F40042E2E5F7E8EF8189FED15519AECE42C3BFA2";
const wchar_t* const ZEROS_32 = L"0000000000000000000000000000000000000000000000000000000000000000";

// ---- strings ----

std::string ToUtf8(const std::wstring& text) {
    int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// ---- files ----

std::wstring NewDir(const std::wstring& name) {
    std::wstring dir = env.tempRoot + L"\\" + name;
    CHECK(CreateDirectoryW(dir.c_str(), nullptr));
    return dir;
}

void Copy(const std::wstring& source, const std::wstring& target) {
    CHECK(CopyFileW(source.c_str(), target.c_str(), TRUE));
}

bool Exists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool WaitForFile(const std::wstring& path, DWORD timeoutMs) {
    for (DWORD waited = 0; waited < timeoutMs; waited += 100) {
        if (Exists(path)) {
            return true;
        }
        Sleep(100);
    }
    return Exists(path);
}

std::string ReadText(const std::wstring& path) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        return std::string();
    }
    std::string text;
    char buffer[4096];
    DWORD read;
    while (ReadFile(file.Get(), buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        text.append(buffer, read);
    }
    return text;
}

void WriteText(const std::wstring& path, const std::string& text) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    CHECK(file);
    DWORD written;
    CHECK(WriteFile(file.Get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr));
}

std::wstring Sha256Hex(const std::wstring& path) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    CHECK(file);
    std::array<BYTE, 32> digest;
    DWORD error;
    CHECK(ComputeSha256(file.Get(), digest, error));
    static const wchar_t DIGITS[] = L"0123456789abcdef";
    std::wstring hex;
    for (BYTE b : digest) {
        hex.push_back(DIGITS[b >> 4]);
        hex.push_back(DIGITS[b & 0xF]);
    }
    return hex;
}

bool CanOpenForWriting(const std::wstring& path) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    return static_cast<bool>(file);
}

// ---- processes ----

Child Start(const std::wstring& exe, const std::vector<std::wstring>& args, bool redirectStdout) {
    std::wstring commandLine = L"\"" + exe + L"\"";
    for (const std::wstring& arg : args) {
        commandLine += L" \"" + arg + L"\"";
    }
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (redirectStdout) {
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
        CHECK(CreatePipe(&readEnd, &writeEnd, &attributes, 0));
        CHECK(SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0));
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdOutput = writeEnd;
    }
    PROCESS_INFORMATION processInfo{};
    BOOL created = CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, redirectStdout, 0, nullptr, nullptr, &startupInfo, &processInfo);
    if (writeEnd != nullptr) {
        CloseHandle(writeEnd);
    }
    CHECK(created);
    CloseHandle(processInfo.hThread);
    Child child;
    child.process.Reset(processInfo.hProcess);
    child.out.Reset(readEnd);
    child.pid = processInfo.dwProcessId;
    return child;
}

std::string ReadStdout(Child& child) {
    std::string text;
    char buffer[256];
    DWORD read;
    while (ReadFile(child.out.Get(), buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        text.append(buffer, read);
    }
    return text;
}

DWORD WaitForExit(Child& child, DWORD timeoutMs) {
    CHECK(WaitForSingleObject(child.process.Get(), timeoutMs) == WAIT_OBJECT_0);
    DWORD exitCode;
    CHECK(GetExitCodeProcess(child.process.Get(), &exitCode));
    return exitCode;
}

void Kill(Child& child) {
    TerminateProcess(child.process.Get(), 99);
    WaitForSingleObject(child.process.Get(), 10000);
}
