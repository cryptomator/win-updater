#pragma once

#include "handle.h"
#include "log.h"

#include <array>
#include <string>

/**
 * Opens a file for reading and lets others only read it, so that nobody can write, delete or rename it while the handle is open.
 *
 * @param path the file to open
 * @param error receives the Win32 error code if opening fails, e.g. ERROR_SHARING_VIOLATION if someone holds it open for writing
 * @return the handle, empty on failure
 */
UniqueHandle OpenLocked(const std::wstring& path, DWORD& error);

/**
 * Computes the SHA-256 digest of a file's content, read through the given handle from the beginning.
 *
 * @param file handle with read access
 * @param digest receives the digest
 * @param error receives the Win32 error code or NTSTATUS on failure
 * @return whether the digest could be computed
 */
bool ComputeSha256(HANDLE file, std::array<BYTE, 32>& digest, DWORD& error);

/**
 * Returns the path of an open file with all links and junctions resolved, without the \\?\ prefix.
 *
 * @param file handle of the open file
 * @param error receives the Win32 error code on failure
 * @return the path, empty on failure
 */
std::wstring FinalPath(HANDLE file, DWORD& error);

// Converts an extended-length path to a regular one: \\?\C:\dir -> C:\dir, \\?\UNC\server\share -> \\server\share. Other paths are returned unchanged.
std::wstring StripExtendedPrefix(const std::wstring& path);

/**
 * Verifies the embedded Authenticode signature of a file through its handle, including the revocation status of the certificate chain, and checks the signer.
 *
 * @param file handle of the locked file
 * @param path the file's final path, used by WinVerifyTrust to pick the subject interface package
 * @param signerEku OID of an extended key usage that the signing certificate must contain
 * @param rootThumbprint SHA-1 thumbprint of the root certificate at which the chain must end
 * @param log receives the reason if the verification fails
 * @return whether the signature is valid and was created by the expected signer
 */
bool VerifySignature(HANDLE file, const std::wstring& path, const std::string& signerEku, const std::array<BYTE, 20>& rootThumbprint, Log& log);
