// Test double for the process the updater waits for, the installer and Cryptomator. Copied under the name of its role (e.g. installer.exe), it
// appends its command line to <FAKE_LOG_DIR>\<own file name>.log, sleeps for the milliseconds given with --sleep (default 0) and exits with the
// code in <FAKE_LOG_DIR>\<own file name>.exitcode (default 0). Without FAKE_LOG_DIR it only sleeps.
#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <string>

namespace {

    void AppendUtf8Line(const std::wstring& path, const std::wstring& line) {
        HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return;
        }
        std::wstring text = line + L"\r\n";
        int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
        DWORD written;
        WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
        CloseHandle(file);
    }

    int ReadExitCode(const std::wstring& path) {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return 0;
        }
        char buffer[16] = {};
        DWORD read = 0;
        ReadFile(file, buffer, sizeof(buffer) - 1, &read, nullptr);
        CloseHandle(file);
        return std::atoi(buffer);
    }

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t self[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring selfPath(self, length);
    std::wstring name = selfPath.substr(selfPath.find_last_of(L'\\') + 1);

    int exitCode = 0;
    wchar_t logDir[MAX_PATH];
    DWORD logDirLength = GetEnvironmentVariableW(L"FAKE_LOG_DIR", logDir, MAX_PATH);
    if (logDirLength > 0 && logDirLength < MAX_PATH) {
        std::wstring base = std::wstring(logDir, logDirLength) + L"\\" + name;
        AppendUtf8Line(base + L".log", GetCommandLineW());
        exitCode = ReadExitCode(base + L".exitcode");
    }

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv != nullptr && i + 1 < argc; ++i) {
        if (std::wstring(argv[i]) == L"--sleep") {
            Sleep(static_cast<DWORD>(_wtoi(argv[i + 1])));
        }
    }
    LocalFree(argv);
    return exitCode;
}
