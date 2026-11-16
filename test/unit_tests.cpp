// Unit tests of the updater's functions that don't need a running process.
#include "testing.h"

#include "../src/options.h"
#include "../src/process.h"
#include "../src/verify.h"

#include <optional>

namespace {

    std::vector<const wchar_t*> ValidArgs() {
        return {L"updater.exe", L"--wait-pid", L"1234", L"--installer", L"C:\\work\\Cryptomator-1.19.6-x64.msi", L"--sha256", L"00112233445566778899aabbccddeeff00112233445566778899AABBCCDDEEFF", L"--signer-eku", L"1.3.6.1.4.1.311.97.1", L"--root-thumbprint", ROOT, L"--relaunch", L"C:\\Program Files\\Cryptomator\\Cryptomator.exe"};
    }

    std::optional<Options> Parse(const std::vector<const wchar_t*>& args, std::wstring& error) {
        return ParseOptions(static_cast<int>(args.size()), args.data(), error);
    }

    // Replaces the value of an argument, or appends the pair if absent.
    std::vector<const wchar_t*> With(const wchar_t* name, const wchar_t* value) {
        std::vector<const wchar_t*> args = ValidArgs();
        for (size_t i = 1; i + 1 < args.size(); i += 2) {
            if (std::wstring(args[i]) == name) {
                args[i + 1] = value;
                return args;
            }
        }
        args.push_back(name);
        args.push_back(value);
        return args;
    }

    // Asserts that parsing fails with the given error, so that a mistyped value can't pass by tripping a different check.
    void ExpectRejected(const std::vector<const wchar_t*>& args, const std::wstring& expectedError) {
        std::wstring error;
        CHECK(!Parse(args, error).has_value());
        CHECK(error.find(expectedError) != std::wstring::npos);
    }

    void ParseOptionsAcceptsValidArguments() {
        std::wstring error;
        std::optional<Options> options = Parse(ValidArgs(), error);
        CHECK(options.has_value());
        CHECK(options->waitPid == 1234);
        CHECK(options->installerType == InstallerType::Msi);
        CHECK(options->sha256[0] == 0x00 && options->sha256[1] == 0x11 && options->sha256[31] == 0xFF);
        CHECK(options->signerEku == "1.3.6.1.4.1.311.97.1");
        CHECK(options->rootThumbprint[0] == 0xF4 && options->rootThumbprint[19] == 0xA2);
        CHECK(options->logDir == L"C:\\work"); // defaults to the installer's directory
    }

    void ParseOptionsDetectsInstallerType() {
        std::wstring error;
        CHECK(Parse(With(L"--installer", L"C:\\work\\setup.EXE"), error)->installerType == InstallerType::Exe);
    }

    void ParseOptionsRejectsMalformedArguments() {
        std::vector<const wchar_t*> missing = ValidArgs();
        missing.resize(missing.size() - 2);
        ExpectRejected(missing, L"missing argument --relaunch");
        std::vector<const wchar_t*> withoutValue = ValidArgs();
        withoutValue.pop_back();
        ExpectRejected(withoutValue, L"missing value for --relaunch");
        ExpectRejected(With(L"--unknown", L"x"), L"unknown argument --unknown");
        std::vector<const wchar_t*> duplicate = ValidArgs();
        duplicate.push_back(L"--wait-pid");
        duplicate.push_back(L"5678");
        ExpectRejected(duplicate, L"duplicate argument --wait-pid");
        ExpectRejected(With(L"--wait-pid", L"0"), L"invalid --wait-pid");
        ExpectRejected(With(L"--wait-pid", L"12a"), L"invalid --wait-pid");
        ExpectRejected(With(L"--installer", L"work\\setup.msi"), L"--installer must be an absolute path");
        ExpectRejected(With(L"--installer", L"C:\\work\\setup\".msi"), L"--installer must be an absolute path");
        ExpectRejected(With(L"--installer", L"C:\\work\\setup.zip"), L"--installer must end with .msi or .exe");
        ExpectRejected(With(L"--sha256", L"0011"), L"--sha256 must be 64 hexadecimal characters");
        ExpectRejected(With(L"--sha256", L"g0112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"), L"--sha256 must be 64 hexadecimal characters");
        ExpectRejected(With(L"--signer-eku", L"1"), L"--signer-eku must be an OID");
        ExpectRejected(With(L"--signer-eku", L"1..3"), L"--signer-eku must be an OID");
        ExpectRejected(With(L"--signer-eku", L"1.a"), L"--signer-eku must be an OID");
        ExpectRejected(With(L"--root-thumbprint", ZEROS_32), L"--root-thumbprint must be 40 hexadecimal characters");
        ExpectRejected(With(L"--relaunch", L"C:\\Program Files\\Cryptomator\\Cryptomator.bat"), L"--relaunch must be an absolute path to an .exe");
        ExpectRejected(With(L"--log-dir", L"C:\\work\\"), L"--log-dir must be an absolute path without trailing backslash");
    }

    void ParentDirectoryKeepsDriveRoot() {
        CHECK(ParentDirectory(L"C:\\Program Files\\Cryptomator\\Cryptomator.exe") == L"C:\\Program Files\\Cryptomator");
        CHECK(ParentDirectory(L"C:\\Cryptomator.exe") == L"C:\\");
        CHECK(ParentDirectory(L"\\\\server\\share\\Cryptomator.exe") == L"\\\\server\\share");
    }

    void StripExtendedPrefixConvertsDriveAndUncPaths() {
        CHECK(StripExtendedPrefix(L"\\\\?\\C:\\work\\setup.msi") == L"C:\\work\\setup.msi");
        CHECK(StripExtendedPrefix(L"\\\\?\\UNC\\server\\share\\setup.msi") == L"\\\\server\\share\\setup.msi");
        CHECK(StripExtendedPrefix(L"\\\\?\\Volume{0}\\setup.msi") == L"\\\\?\\Volume{0}\\setup.msi");
        CHECK(StripExtendedPrefix(L"C:\\work\\setup.msi") == L"C:\\work\\setup.msi");
    }

    void InstallerCommandLinesMatchTheSpec() {
        CHECK(BuildMsiCommandLine(L"C:\\Windows\\System32\\msiexec.exe", L"C:\\work\\setup.msi", L"C:\\Program Files\\Cryptomator", L"C:\\work\\installer.log")
              == L"\"C:\\Windows\\System32\\msiexec.exe\" /i \"C:\\work\\setup.msi\" /passive /norestart INSTALLDIR=\"C:\\Program Files\\Cryptomator\" /l*v \"C:\\work\\installer.log\"");
        CHECK(BuildBundleParameters(L"C:\\work\\installer.log") == L"/passive /norestart /log \"C:\\work\\installer.log\"");
    }

}

std::vector<TestCase> UnitTests() {
    return {
        {"ParseOptionsAcceptsValidArguments", ParseOptionsAcceptsValidArguments},
        {"ParseOptionsDetectsInstallerType", ParseOptionsDetectsInstallerType},
        {"ParseOptionsRejectsMalformedArguments", ParseOptionsRejectsMalformedArguments},
        {"ParentDirectoryKeepsDriveRoot", ParentDirectoryKeepsDriveRoot},
        {"StripExtendedPrefixConvertsDriveAndUncPaths", StripExtendedPrefixConvertsDriveAndUncPaths},
        {"InstallerCommandLinesMatchTheSpec", InstallerCommandLinesMatchTheSpec},
    };
}
