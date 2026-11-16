#include "updater.h"

#include "log.h"
#include "options.h"
#include "process.h"
#include "verify.h"

#include <optional>

// How long to wait for the app to exit; shortened in the test build.
#ifndef WIN_UPDATER_WAIT_TIMEOUT_MS
#define WIN_UPDATER_WAIT_TIMEOUT_MS 60000
#endif

namespace {

    std::wstring ToHex(const BYTE* bytes, size_t length) {
        static const wchar_t DIGITS[] = L"0123456789abcdef";
        std::wstring hex;
        for (size_t i = 0; i < length; ++i) {
            hex.push_back(DIGITS[bytes[i] >> 4]);
            hex.push_back(DIGITS[bytes[i] & 0xF]);
        }
        return hex;
    }

    // Writes READY to stdout and closes it. Fails if the caller is gone (e.g. ERROR_NO_DATA when it closed the pipe) or there is no stdout at all.
    bool Handshake(Log& log) {
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (out == nullptr || out == INVALID_HANDLE_VALUE) {
            log.Info(L"No stdout to report READY to, not updating");
            return false;
        }
        static const char READY[] = "READY\n";
        DWORD written = 0;
        bool ok = WriteFile(out, READY, sizeof(READY) - 1, &written, nullptr) && written == sizeof(READY) - 1;
        DWORD error = ok ? ERROR_SUCCESS : GetLastError();
        CloseHandle(out);
        SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
        if (!ok) {
            log.Error(error, L"Failed to report READY, the caller is gone, not updating");
        }
        return ok;
    }

}

ExitCode Run(int argc, const wchar_t* const* argv) {
    // Phase 1, step 1: leave the install dir, which the upgrade removes and recreates (DLL loading is restricted in wWinMain already)
    std::wstring system32 = SystemDirectory();
    SetCurrentDirectoryW(system32.c_str());

    std::wstring error;
    std::optional<Options> parsed = ParseOptions(argc, argv, error);
    if (!parsed) {
        return ExitCode::InvalidArguments; // without a valid log dir, there is nowhere to log to
    }
    const Options& options = *parsed;
    Log log;
    log.Open(options.logDir + L"\\win-updater.log");
    log.Info(L"Updating with %ls, waiting for process %lu, relaunching %ls", options.installer.c_str(), options.waitPid, options.relaunch.c_str());

    // the caller waits for the handshake, so the PID still belongs to it and the handle prevents its reuse
    UniqueHandle target(OpenProcess(SYNCHRONIZE, FALSE, options.waitPid));
    if (!target) {
        log.Error(GetLastError(), L"Failed to open process %lu", options.waitPid);
        return ExitCode::ProcessNotFound;
    }

    DWORD win32Error;
    UniqueHandle installer = OpenLocked(options.installer, win32Error);
    if (!installer) {
        log.Error(win32Error, L"Failed to open and lock %ls", options.installer.c_str());
        return ExitCode::InstallerNotLockable;
    }
    std::wstring finalPath = FinalPath(installer.Get(), win32Error);
    if (finalPath.empty()) {
        log.Error(win32Error, L"Failed to resolve the final path of %ls", options.installer.c_str());
        return ExitCode::InternalError;
    }

    std::array<BYTE, 32> digest;
    if (!ComputeSha256(installer.Get(), digest, win32Error)) {
        log.Error(win32Error, L"Failed to compute the SHA-256 digest of %ls", finalPath.c_str());
        return ExitCode::InternalError;
    }
    if (digest != options.sha256) {
        log.Info(L"Digest mismatch for %ls: expected %ls, actual %ls", finalPath.c_str(), ToHex(options.sha256.data(), options.sha256.size()).c_str(), ToHex(digest.data(), digest.size()).c_str());
        return ExitCode::DigestMismatch;
    }
    if (!VerifySignature(installer.Get(), finalPath, options.signerEku, options.rootThumbprint, log)) {
        return ExitCode::SignatureInvalid;
    }
    log.Info(L"Verified digest and signature of %ls", finalPath.c_str());

    if (!Handshake(log)) {
        return ExitCode::HandshakeFailed;
    }

    // Phase 2
    DWORD waitResult = WaitForSingleObject(target.Get(), WIN_UPDATER_WAIT_TIMEOUT_MS);
    if (waitResult == WAIT_TIMEOUT) {
        // the app is still running, so neither install nor relaunch
        log.Info(L"Process %lu did not exit within %lu ms, not updating", options.waitPid, static_cast<DWORD>(WIN_UPDATER_WAIT_TIMEOUT_MS));
        return ExitCode::WaitTimeout;
    }
    if (waitResult != WAIT_OBJECT_0) {
        log.Error(GetLastError(), L"Failed to wait for process %lu", options.waitPid);
        return ExitCode::InternalError;
    }
    log.Info(L"Process %lu exited", options.waitPid);

    std::wstring installerLog = options.logDir + L"\\installer.log";
    DWORD installerExitCode = 0;
    bool installerRan;
    if (options.installerType == InstallerType::Msi) {
        std::wstring msiexec = system32 + L"\\msiexec.exe";
        std::wstring commandLine = BuildMsiCommandLine(msiexec, finalPath, ParentDirectory(options.relaunch), installerLog);
        log.Info(L"Running %ls", commandLine.c_str());
        installerRan = RunMsiexec(msiexec, commandLine, system32, installerExitCode, log);
    } else {
        std::wstring parameters = BuildBundleParameters(installerLog);
        log.Info(L"Running %ls %ls elevated", finalPath.c_str(), parameters.c_str());
        installerRan = RunElevated(finalPath, parameters, system32, installerExitCode, log);
    }
    installer.Reset();
    if (installerRan) {
        log.Info(L"Installer exited with code %lu", installerExitCode);
    }

    // the app has exited, so bring it back in any case: updated, or the old version after a declined UAC prompt or a failed installation
    log.Info(L"Relaunching %ls", options.relaunch.c_str());
    bool relaunched = Launch(options.relaunch, log);
    return installerRan && relaunched ? ExitCode::Ok : ExitCode::InternalError;
}
