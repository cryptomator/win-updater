#pragma once

#include "handle.h"

#include <string>

// Appends timestamped UTF-8 lines to win-updater.log. Logging is best effort: write errors are ignored.
class Log {
public:
    // Opens (or creates) the log file; without a successful call, all messages are discarded.
    void Open(const std::wstring& path);

    void Info(const wchar_t* format, ...);

    // Logs the message followed by the Win32 error code and its system message.
    void Error(DWORD win32Error, const wchar_t* format, ...);

private:
    UniqueHandle file_;
    void Write(const wchar_t* level, const std::wstring& message);
};
