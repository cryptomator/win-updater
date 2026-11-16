#include "log.h"

#include <cstdarg>
#include <cwchar>

namespace {

    std::wstring FormatV(const wchar_t* format, va_list args) {
        wchar_t buffer[2048];
        int length = _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
        return length < 0 ? std::wstring(buffer) + L"..." : std::wstring(buffer, static_cast<size_t>(length));
    }

    std::wstring SystemMessage(DWORD error) {
        wchar_t* text = nullptr;
        DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
        std::wstring message = length == 0 ? std::wstring() : std::wstring(text, length);
        LocalFree(text);
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
            message.pop_back();
        }
        return message;
    }

}

void Log::Open(const std::wstring& path) {
    file_.Reset(CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
}

void Log::Info(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    std::wstring message = FormatV(format, args);
    va_end(args);
    Write(L"INFO ", message);
}

void Log::Error(DWORD win32Error, const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    std::wstring message = FormatV(format, args);
    va_end(args);
    wchar_t code[32];
    swprintf_s(code, L" (error %lu: ", win32Error);
    Write(L"ERROR", message + code + SystemMessage(win32Error) + L")");
}

void Log::Write(const wchar_t* level, const std::wstring& message) {
    if (!file_) {
        return;
    }
    // UTC in ISO 8601, formatted by hand, so that neither locale nor calendar can change it
    SYSTEMTIME now;
    GetSystemTime(&now);
    wchar_t timestamp[32];
    swprintf_s(timestamp, L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    std::wstring line = std::wstring(timestamp) + L" " + level + L" " + message + L"\r\n";

    int size = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return;
    }
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), utf8.data(), size, nullptr, nullptr);
    DWORD written;
    WriteFile(file_.Get(), utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}
