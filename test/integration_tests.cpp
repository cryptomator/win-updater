// Integration tests that run the updater as a process: phase 1 with the release build (real verification), phase 2 with the test build (stub verifier,
// `open` instead of `runas`, 3 s timeout). fake.exe plays the process to wait for, the installer and Cryptomator.
#include "testing.h"

namespace {

    // A process for the updater to wait for.
    Child StartWaitTarget() {
        return Start(env.fake, {L"--sleep", L"600000"}, false);
    }

    std::vector<std::wstring> UpdaterArgs(DWORD pid, const std::wstring& installer, const std::wstring& sha256, const std::wstring& relaunch, const std::wstring& logDir, const std::wstring& eku = EKU, const std::wstring& root = ROOT) {
        return {L"--wait-pid", std::to_wstring(pid), L"--installer", installer, L"--sha256", sha256, L"--signer-eku", eku, L"--root-thumbprint", root, L"--relaunch", relaunch, L"--log-dir", logDir};
    }

    // ---- phase 1 with the release build ----

    // Runs the release updater on an installer that must fail verification; returns its exit code and log.
    DWORD RunRejected(const std::wstring& dir, const std::wstring& installer, const std::wstring& sha256, std::string& log, const std::wstring& eku = EKU, const std::wstring& root = ROOT) {
        Child target = StartWaitTarget();
        Child updater = Start(env.updater, UpdaterArgs(target.pid, installer, sha256, dir + L"\\app.exe", dir, eku, root), true);
        std::string out = ReadStdout(updater);
        DWORD exitCode = WaitForExit(updater,60000);
        Kill(target);
        CHECK(out.empty());
        log = ReadText(dir + L"\\win-updater.log");
        return exitCode;
    }

    void VerifiedInstallerIsReportedAndStaysLocked() {
        std::wstring dir = NewDir(L"verified");
        std::wstring installer = dir + L"\\installer.exe";
        Copy(env.fixture, installer);
        Child target = StartWaitTarget();
        Child updater = Start(env.updater, UpdaterArgs(target.pid, installer, Sha256Hex(installer), dir + L"\\app.exe", dir), true);
        std::string out = ReadStdout(updater);
        bool locked = !CanOpenForWriting(installer);
        // stop the updater before the target exits, as it would otherwise start the fixture elevated
        Kill(updater);
        Kill(target);
        CHECK(out == "READY\n");
        CHECK(locked);
        CHECK(Contains(ReadText(dir + L"\\win-updater.log"), "Verified digest and signature"));
    }

    void DigestMismatchIsRejected() {
        std::wstring dir = NewDir(L"digest");
        std::wstring installer = dir + L"\\installer.exe";
        Copy(env.fixture, installer);
        std::string log;
        CHECK(RunRejected(dir, installer, ZEROS_32, log) == 5);
        CHECK(Contains(log, "Digest mismatch"));
    }

    void UnsignedInstallerIsRejected() {
        std::wstring dir = NewDir(L"unsigned");
        std::wstring installer = dir + L"\\installer.exe";
        Copy(env.fake, installer);
        std::string log;
        CHECK(RunRejected(dir, installer, Sha256Hex(installer), log) == 6);
        CHECK(Contains(log, "WinVerifyTrust rejected"));
    }

    void TamperedInstallerIsRejected() {
        std::wstring dir = NewDir(L"tampered");
        std::wstring installer = dir + L"\\installer.exe";
        Copy(env.fixture, installer);
        {
            UniqueHandle file(CreateFileW(installer.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            CHECK(file);
            LARGE_INTEGER offset{};
            offset.QuadPart = 0x2000; // inside the code section, covered by the signature
            CHECK(SetFilePointerEx(file.Get(), offset, nullptr, FILE_BEGIN));
            BYTE b;
            DWORD transferred;
            CHECK(ReadFile(file.Get(), &b, 1, &transferred, nullptr) && transferred == 1);
            b ^= 0xFF;
            CHECK(SetFilePointerEx(file.Get(), offset, nullptr, FILE_BEGIN));
            CHECK(WriteFile(file.Get(), &b, 1, &transferred, nullptr) && transferred == 1);
        }
        std::string log;
        CHECK(RunRejected(dir, installer, Sha256Hex(installer), log) == 6);
        CHECK(Contains(log, "WinVerifyTrust rejected"));
    }

    void WrongSignerEkuIsRejected() {
        std::wstring dir = NewDir(L"eku");
        std::wstring installer = dir + L"\\installer.exe";
        Copy(env.fixture, installer);
        std::string log;
        CHECK(RunRejected(dir, installer, Sha256Hex(installer), log, L"1.3.6.1.4.1.311.97.1") == 6);
        CHECK(Contains(log, "lacks the extended key usage 1.3.6.1.4.1.311.97.1"));
    }

    void WrongRootIsRejected() {
        std::wstring dir = NewDir(L"root");
        std::wstring installer = dir + L"\\installer.exe";
        Copy(env.fixture, installer);
        std::string log;
        CHECK(RunRejected(dir, installer, Sha256Hex(installer), log, EKU, L"0000000000000000000000000000000000000000") == 6);
        CHECK(Contains(log, "ends at an unexpected root"));
    }

    void InstallerOpenForWritingIsRejected() {
        std::wstring dir = NewDir(L"writer");
        std::wstring installer = dir + L"\\installer.exe";
        Copy(env.fixture, installer);
        std::wstring sha256 = Sha256Hex(installer); // before the writer exists, as Sha256Hex itself denies write sharing
        UniqueHandle writer(CreateFileW(installer.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        CHECK(writer);
        std::string log;
        CHECK(RunRejected(dir, installer, sha256, log) == 4);
        CHECK(Contains(log, "Failed to open and lock"));
    }

    void MissingProcessIsRejected() {
        std::wstring dir = NewDir(L"noprocess");
        std::wstring installer = dir + L"\\installer.exe";
        Copy(env.fixture, installer);
        Child gone = Start(env.fake, {}, false);
        WaitForExit(gone, 10000);
        DWORD pid = gone.pid;
        gone.process.Reset(); // releases the PID
        Child updater = Start(env.updater, UpdaterArgs(pid, installer, Sha256Hex(installer), dir + L"\\app.exe", dir), true);
        std::string out = ReadStdout(updater);
        CHECK(WaitForExit(updater,60000) == 3);
        CHECK(out.empty());
        CHECK(Contains(ReadText(dir + L"\\win-updater.log"), "Failed to open process"));
    }

    void InvalidArgumentsAreRejected() {
        Child updater = Start(env.updater, {L"--wait-pid", L"1"}, true);
        std::string out = ReadStdout(updater);
        CHECK(WaitForExit(updater,10000) == 2);
        CHECK(out.empty());
    }

    void CallerGoneBeforeReadyAborts() {
        std::wstring dir = NewDir(L"callergone");
        std::wstring installer = dir + L"\\installer.exe";
        Copy(env.fixture, installer);
        Child target = StartWaitTarget();
        Child updater = Start(env.updater, UpdaterArgs(target.pid, installer, Sha256Hex(installer), dir + L"\\app.exe", dir), true);
        updater.out.Reset(); // the caller stops listening before the updater reports READY
        DWORD exitCode = WaitForExit(updater,60000);
        Kill(target);
        CHECK(exitCode == 8);
        CHECK(Contains(ReadText(dir + L"\\win-updater.log"), "Failed to report READY"));
    }

    // ---- phase 2 with the test build ----

    struct Phase2Setup {
        std::wstring dir;
        std::wstring installer;
        std::wstring relaunch;
    };

    Phase2Setup PreparePhase2(const std::wstring& name) {
        Phase2Setup setup;
        setup.dir = NewDir(name);
        setup.installer = setup.dir + L"\\installer.exe";
        setup.relaunch = setup.dir + L"\\app.exe";
        Copy(env.fake, setup.installer);
        Copy(env.fake, setup.relaunch);
        // inherited by the updater and from there by the fake installer and the relaunched fake app
        CHECK(SetEnvironmentVariableW(L"FAKE_LOG_DIR", setup.dir.c_str()));
        return setup;
    }

    void InstallsAfterExitAndRelaunches() {
        Phase2Setup setup = PreparePhase2(L"install");
        Child target = StartWaitTarget();
        Child updater = Start(env.testUpdater, UpdaterArgs(target.pid, setup.installer, Sha256Hex(setup.installer), setup.relaunch, setup.dir), true);
        CHECK(ReadStdout(updater) == "READY\n");
        CHECK(!Exists(setup.installer + L".log")); // nothing runs while the target is alive
        Kill(target);
        CHECK(WaitForExit(updater,30000) == 0);
        std::string installerCall = ReadText(setup.installer + L".log");
        CHECK(Contains(installerCall, ToUtf8(setup.installer))); // the final path, which equals the given one here
        CHECK(Contains(installerCall, ToUtf8(L"/passive /norestart /log \"" + setup.dir + L"\\installer.log\"")));
        CHECK(WaitForFile(setup.relaunch + L".log", 10000));
        CHECK(Contains(ReadText(setup.dir + L"\\win-updater.log"), "Installer exited with code 0"));
    }

    void RelaunchesAfterFailedInstallation() {
        Phase2Setup setup = PreparePhase2(L"failedinstall");
        WriteText(setup.installer + L".exitcode", "1603");
        Child target = StartWaitTarget();
        Child updater = Start(env.testUpdater, UpdaterArgs(target.pid, setup.installer, Sha256Hex(setup.installer), setup.relaunch, setup.dir), true);
        CHECK(ReadStdout(updater) == "READY\n");
        Kill(target);
        CHECK(WaitForExit(updater,30000) == 0);
        CHECK(Contains(ReadText(setup.dir + L"\\win-updater.log"), "Installer exited with code 1603"));
        CHECK(WaitForFile(setup.relaunch + L".log", 10000));
    }

    void NeitherInstallsNorRelaunchesOnTimeout() {
        Phase2Setup setup = PreparePhase2(L"timeout");
        Child target = StartWaitTarget();
        Child updater = Start(env.testUpdater, UpdaterArgs(target.pid, setup.installer, Sha256Hex(setup.installer), setup.relaunch, setup.dir), true);
        CHECK(ReadStdout(updater) == "READY\n");
        DWORD exitCode = WaitForExit(updater,30000); // the test build waits 3 s instead of 60 s
        Kill(target);
        CHECK(exitCode == 7);
        CHECK(!Exists(setup.installer + L".log"));
        Sleep(1000);
        CHECK(!Exists(setup.relaunch + L".log"));
    }

}

std::vector<TestCase> IntegrationTests() {
    return {
        {"VerifiedInstallerIsReportedAndStaysLocked", VerifiedInstallerIsReportedAndStaysLocked},
        {"DigestMismatchIsRejected", DigestMismatchIsRejected},
        {"UnsignedInstallerIsRejected", UnsignedInstallerIsRejected},
        {"TamperedInstallerIsRejected", TamperedInstallerIsRejected},
        {"WrongSignerEkuIsRejected", WrongSignerEkuIsRejected},
        {"WrongRootIsRejected", WrongRootIsRejected},
        {"InstallerOpenForWritingIsRejected", InstallerOpenForWritingIsRejected},
        {"MissingProcessIsRejected", MissingProcessIsRejected},
        {"InvalidArgumentsAreRejected", InvalidArgumentsAreRejected},
        {"CallerGoneBeforeReadyAborts", CallerGoneBeforeReadyAborts},
        {"InstallsAfterExitAndRelaunches", InstallsAfterExitAndRelaunches},
        {"RelaunchesAfterFailedInstallation", RelaunchesAfterFailedInstallation},
        {"NeitherInstallsNorRelaunchesOnTimeout", NeitherInstallsNorRelaunchesOnTimeout},
    };
}
