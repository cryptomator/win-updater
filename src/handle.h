#pragma once

#include <windows.h>

// Owns a kernel handle and closes it on destruction. Treats both NULL and INVALID_HANDLE_VALUE as "no handle".
class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) : handle_(handle == INVALID_HANDLE_VALUE ? nullptr : handle) {}
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.Release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        Reset(other.Release());
        return *this;
    }
    ~UniqueHandle() { Reset(); }

    HANDLE Get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

    HANDLE Release() {
        HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    void Reset(HANDLE handle = nullptr) {
        if (handle_ != nullptr) {
            CloseHandle(handle_);
        }
        handle_ = handle == INVALID_HANDLE_VALUE ? nullptr : handle;
    }

private:
    HANDLE handle_ = nullptr;
};
