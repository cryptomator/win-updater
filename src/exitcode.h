#pragma once

// Exit codes of the updater, see "Exit codes" in docs/spec.md.
enum class ExitCode : int {
    Ok = 0,
    InternalError = 1,
    InvalidArguments = 2,
    ProcessNotFound = 3,
    InstallerNotLockable = 4,
    DigestMismatch = 5,
    SignatureInvalid = 6,
    WaitTimeout = 7,
    HandshakeFailed = 8,
};
