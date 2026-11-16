#pragma once

#include <windows.h>

#include <array>
#include <optional>
#include <string>

enum class InstallerType { Msi, Exe };

// Validated command-line arguments, see "Invocation" in docs/spec.md.
struct Options {
    DWORD waitPid = 0;
    std::wstring installer;
    InstallerType installerType = InstallerType::Msi;
    std::array<BYTE, 32> sha256{};
    std::string signerEku;
    std::array<BYTE, 20> rootThumbprint{};
    std::wstring relaunch;
    std::wstring logDir;
};

/**
 * Parses and validates the command-line arguments.
 *
 * @param argc number of arguments, including the program name
 * @param argv arguments, argv[0] being the program name
 * @param error receives a description of the first problem if parsing fails
 * @return the options, or std::nullopt if an argument is unknown, duplicate, missing or malformed
 */
std::optional<Options> ParseOptions(int argc, const wchar_t* const* argv, std::wstring& error);

// Directory part of a path without trailing backslash, e.g. C:\dir\file.exe -> C:\dir, C:\file.exe -> C:\.
std::wstring ParentDirectory(const std::wstring& path);
