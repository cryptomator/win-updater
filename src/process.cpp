#include "process.h"

#include "options.h"

#include <objbase.h>
#include <shellapi.h>

#include <vector>

#ifdef WIN_UPDATER_TEST
// Test build only: starts test doubles without a UAC prompt.
static const wchar_t* const ELEVATION_VERB = L"open";
#else
static const wchar_t* const ELEVATION_VERB = L"runas";
#endif

std::wstring SystemDirectory() {
    wchar_t buffer[MAX_PATH];
    UINT length = GetSystemDirectoryW(buffer, MAX_PATH);
    return length == 0 || length >= MAX_PATH ? std::wstring(L"C:\\Windows\\System32") : std::wstring(buffer, length);
}

std::wstring BuildMsiCommandLine(const std::wstring& msiexec, const std::wstring& package, const std::wstring& installDir, const std::wstring& logFile) {
    return L"\"" + msiexec + L"\" /i \"" + package + L"\" /passive /norestart INSTALLDIR=\"" + installDir + L"\" /l*v \"" + logFile + L"\"";
}

std::wstring BuildBundleParameters(const std::wstring& logFile) {
    return L"/passive /norestart /log \"" + logFile + L"\"";
}

bool RunMsiexec(const std::wstring& msiexec, std::wstring commandLine, const std::wstring& workingDir, DWORD& exitCode, Log& log) {
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    // the application name is given explicitly, so that no search path is involved; no handles are inherited
    if (!CreateProcessW(msiexec.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, workingDir.c_str(), &startupInfo, &processInfo)) {
        log.Error(GetLastError(), L"Failed to start %ls", commandLine.c_str());
        return false;
    }
    UniqueHandle process(processInfo.hProcess);
    UniqueHandle thread(processInfo.hThread);
    WaitForSingleObject(process.Get(), INFINITE);
    if (!GetExitCodeProcess(process.Get(), &exitCode)) {
        log.Error(GetLastError(), L"Failed to get the exit code of msiexec");
        return false;
    }
    return true;
}

bool RunElevated(const std::wstring& bundle, const std::wstring& parameters, const std::wstring& workingDir, DWORD& exitCode, Log& log) {
    // recommended for ShellExecuteEx, as it may delegate to COM-based shell extensions
    HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = ELEVATION_VERB;
    info.lpFile = bundle.c_str();
    info.lpParameters = parameters.c_str();
    info.lpDirectory = workingDir.c_str();
    info.nShow = SW_SHOWNORMAL;
    bool started = ShellExecuteExW(&info) != FALSE;
    DWORD error = started ? ERROR_SUCCESS : GetLastError();
    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }

    if (!started) {
        if (error == ERROR_CANCELLED) {
            log.Info(L"UAC prompt for %ls was declined", bundle.c_str());
            exitCode = ERROR_CANCELLED;
            return true;
        }
        log.Error(error, L"Failed to start %ls", bundle.c_str());
        return false;
    }
    if (info.hProcess == nullptr) {
        log.Info(L"Started %ls, but got no process handle to wait for", bundle.c_str());
        return false;
    }
    UniqueHandle process(info.hProcess);
    WaitForSingleObject(process.Get(), INFINITE);
    if (!GetExitCodeProcess(process.Get(), &exitCode)) {
        log.Error(GetLastError(), L"Failed to get the exit code of %ls", bundle.c_str());
        return false;
    }
    return true;
}

bool Launch(const std::wstring& exe, Log& log) {
    std::wstring commandLine = L"\"" + exe + L"\"";
    std::wstring workingDir = ParentDirectory(exe);
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, workingDir.c_str(), &startupInfo, &processInfo)) {
        log.Error(GetLastError(), L"Failed to start %ls", exe.c_str());
        return false;
    }
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}
