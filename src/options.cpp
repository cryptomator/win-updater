#include "options.h"

#include <cwctype>
#include <map>

namespace {

    bool EndsWithIgnoreCase(const std::wstring& value, const std::wstring& suffix) {
        if (value.size() < suffix.size()) {
            return false;
        }
        return CompareStringOrdinal(value.c_str() + value.size() - suffix.size(), static_cast<int>(suffix.size()), suffix.c_str(), static_cast<int>(suffix.size()), TRUE) == CSTR_EQUAL;
    }

    // Accepts drive paths (C:\...) and UNC paths (\\server\share\...), but not relative or drive-relative ones, as the updater changes its working directory.
    bool IsFullyQualified(const std::wstring& path) {
        if (path.size() >= 3 && std::iswalpha(path[0]) && path[1] == L':' && path[2] == L'\\') {
            return true;
        }
        return path.size() >= 3 && path[0] == L'\\' && path[1] == L'\\' && path[2] != L'?' && path[2] != L'.';
    }

    // Paths are passed on to command lines in double quotes, which Windows file names can't contain anyway.
    bool IsAcceptablePath(const std::wstring& path) {
        return IsFullyQualified(path) && path.find(L'"') == std::wstring::npos && path.back() != L'\\';
    }

    template <size_t N>
    bool ParseHex(const std::wstring& hex, std::array<BYTE, N>& bytes) {
        if (hex.size() != N * 2) {
            return false;
        }
        for (size_t i = 0; i < N; ++i) {
            BYTE value = 0;
            for (size_t j = 0; j < 2; ++j) {
                wchar_t c = hex[i * 2 + j];
                int nibble;
                if (c >= L'0' && c <= L'9') {
                    nibble = c - L'0';
                } else if (c >= L'a' && c <= L'f') {
                    nibble = c - L'a' + 10;
                } else if (c >= L'A' && c <= L'F') {
                    nibble = c - L'A' + 10;
                } else {
                    return false;
                }
                value = static_cast<BYTE>(value << 4 | nibble);
            }
            bytes[i] = value;
        }
        return true;
    }

    // An OID like 1.3.6.1.4.1.311: at least two numeric components separated by single dots.
    bool ParseOid(const std::wstring& oid, std::string& ascii) {
        if (oid.empty() || oid.front() == L'.' || oid.back() == L'.' || oid.find(L"..") != std::wstring::npos || oid.find(L'.') == std::wstring::npos) {
            return false;
        }
        ascii.clear();
        for (wchar_t c : oid) {
            if (c != L'.' && (c < L'0' || c > L'9')) {
                return false;
            }
            ascii.push_back(static_cast<char>(c));
        }
        return true;
    }

    bool ParsePid(const std::wstring& text, DWORD& pid) {
        if (text.empty() || text.size() > 10) {
            return false;
        }
        unsigned long long value = 0;
        for (wchar_t c : text) {
            if (c < L'0' || c > L'9') {
                return false;
            }
            value = value * 10 + (c - L'0');
        }
        if (value == 0 || value > MAXDWORD) {
            return false;
        }
        pid = static_cast<DWORD>(value);
        return true;
    }

}

std::wstring ParentDirectory(const std::wstring& path) {
    size_t separator = path.find_last_of(L'\\');
    if (separator == std::wstring::npos) {
        return std::wstring();
    }
    // keep the backslash of a drive root, e.g. C:\file.exe -> C:\ (C: alone would mean the current directory on drive C)
    if (separator == 2 && path[1] == L':') {
        return path.substr(0, 3);
    }
    return path.substr(0, separator);
}

std::optional<Options> ParseOptions(int argc, const wchar_t* const* argv, std::wstring& error) {
    static const wchar_t* const KNOWN[] = {L"--wait-pid", L"--installer", L"--sha256", L"--signer-eku", L"--root-thumbprint", L"--relaunch", L"--log-dir"};
    std::map<std::wstring, std::wstring> values;
    for (int i = 1; i < argc; i += 2) {
        std::wstring name = argv[i];
        bool known = false;
        for (const wchar_t* candidate : KNOWN) {
            known = known || name == candidate;
        }
        if (!known) {
            error = L"unknown argument " + name;
            return std::nullopt;
        }
        if (i + 1 >= argc) {
            error = L"missing value for " + name;
            return std::nullopt;
        }
        if (!values.emplace(name, argv[i + 1]).second) {
            error = L"duplicate argument " + name;
            return std::nullopt;
        }
    }
    for (const wchar_t* name : KNOWN) {
        if (std::wstring(name) != L"--log-dir" && values.count(name) == 0) {
            error = std::wstring(L"missing argument ") + name;
            return std::nullopt;
        }
    }

    Options options;
    if (!ParsePid(values[L"--wait-pid"], options.waitPid)) {
        error = L"invalid --wait-pid";
        return std::nullopt;
    }
    options.installer = values[L"--installer"];
    if (!IsAcceptablePath(options.installer)) {
        error = L"--installer must be an absolute path";
        return std::nullopt;
    }
    if (EndsWithIgnoreCase(options.installer, L".msi")) {
        options.installerType = InstallerType::Msi;
    } else if (EndsWithIgnoreCase(options.installer, L".exe")) {
        options.installerType = InstallerType::Exe;
    } else {
        error = L"--installer must end with .msi or .exe";
        return std::nullopt;
    }
    if (!ParseHex(values[L"--sha256"], options.sha256)) {
        error = L"--sha256 must be 64 hexadecimal characters";
        return std::nullopt;
    }
    if (!ParseOid(values[L"--signer-eku"], options.signerEku)) {
        error = L"--signer-eku must be an OID";
        return std::nullopt;
    }
    if (!ParseHex(values[L"--root-thumbprint"], options.rootThumbprint)) {
        error = L"--root-thumbprint must be 40 hexadecimal characters";
        return std::nullopt;
    }
    options.relaunch = values[L"--relaunch"];
    if (!IsAcceptablePath(options.relaunch) || !EndsWithIgnoreCase(options.relaunch, L".exe")) {
        error = L"--relaunch must be an absolute path to an .exe";
        return std::nullopt;
    }
    if (values.count(L"--log-dir") != 0) {
        options.logDir = values[L"--log-dir"];
        if (!IsAcceptablePath(options.logDir)) {
            error = L"--log-dir must be an absolute path without trailing backslash";
            return std::nullopt;
        }
    } else {
        options.logDir = ParentDirectory(options.installer);
    }
    return options;
}
