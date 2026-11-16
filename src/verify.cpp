#include "verify.h"

#include <bcrypt.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <cstring>
#include <vector>

UniqueHandle OpenLocked(const std::wstring& path, DWORD& error) {
    // no FILE_SHARE_WRITE and no FILE_SHARE_DELETE: others can neither write nor delete or rename the file while we hold it open
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    error = file ? ERROR_SUCCESS : GetLastError();
    return file;
}

bool ComputeSha256(HANDLE file, std::array<BYTE, 32>& digest, DWORD& error) {
    LARGE_INTEGER start{};
    if (!SetFilePointerEx(file, start, nullptr, FILE_BEGIN)) {
        error = GetLastError();
        return false;
    }
    BCRYPT_HASH_HANDLE hash = nullptr;
    NTSTATUS status = BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &hash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        error = static_cast<DWORD>(status);
        return false;
    }
    std::vector<BYTE> buffer(64 * 1024);
    bool ok = true;
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            error = GetLastError();
            ok = false;
            break;
        }
        if (read == 0) {
            break;
        }
        status = BCryptHashData(hash, buffer.data(), read, 0);
        if (!BCRYPT_SUCCESS(status)) {
            error = static_cast<DWORD>(status);
            ok = false;
            break;
        }
    }
    if (ok) {
        status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
        if (!BCRYPT_SUCCESS(status)) {
            error = static_cast<DWORD>(status);
            ok = false;
        }
    }
    BCryptDestroyHash(hash);
    // leave the file pointer at the start for whoever reads next
    SetFilePointerEx(file, start, nullptr, FILE_BEGIN);
    return ok;
}

std::wstring StripExtendedPrefix(const std::wstring& path) {
    if (path.compare(0, 8, L"\\\\?\\UNC\\") == 0) {
        return L"\\\\" + path.substr(8);
    }
    if (path.compare(0, 4, L"\\\\?\\") == 0 && path.size() >= 7 && path[5] == L':' && path[6] == L'\\') {
        return path.substr(4);
    }
    return path;
}

std::wstring FinalPath(HANDLE file, DWORD& error) {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        DWORD length = GetFinalPathNameByHandleW(file, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (length == 0) {
            error = GetLastError();
            return std::wstring();
        }
        if (length < buffer.size()) {
            error = ERROR_SUCCESS;
            return StripExtendedPrefix(std::wstring(buffer.data(), length));
        }
        buffer.resize(length); // too small: length includes the terminating null
    }
}

#ifdef WIN_UPDATER_TEST

// Test build only: accepts every file, so that phase 2 can be tested with unsigned test doubles.
bool VerifySignature(HANDLE, const std::wstring& path, const std::string&, const std::array<BYTE, 20>&, Log& log) {
    log.Info(L"TEST BUILD: skipping signature verification of %ls", path.c_str());
    return true;
}

#else

namespace {

    bool HasEku(PCCERT_CONTEXT cert, const std::string& eku) {
        DWORD size = 0;
        // only the certificate's extension counts, not usages assigned locally through certificate properties
        if (!CertGetEnhancedKeyUsage(cert, CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, nullptr, &size)) {
            return false;
        }
        std::vector<BYTE> buffer(size);
        auto usage = reinterpret_cast<PCERT_ENHKEY_USAGE>(buffer.data());
        if (!CertGetEnhancedKeyUsage(cert, CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, usage, &size)) {
            return false;
        }
        for (DWORD i = 0; i < usage->cUsageIdentifier; ++i) {
            if (eku == usage->rgpszUsageIdentifier[i]) {
                return true;
            }
        }
        return false;
    }

    bool HasThumbprint(PCCERT_CONTEXT cert, const std::array<BYTE, 20>& thumbprint) {
        BYTE actual[20];
        DWORD size = sizeof(actual);
        return CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, actual, &size) && size == sizeof(actual) && std::memcmp(actual, thumbprint.data(), sizeof(actual)) == 0;
    }

    // Checks signer and root of the chain that WinVerifyTrust built and verified.
    bool CheckSigner(HANDLE stateData, const std::wstring& path, const std::string& signerEku, const std::array<BYTE, 20>& rootThumbprint, Log& log) {
        CRYPT_PROVIDER_DATA* providerData = WTHelperProvDataFromStateData(stateData);
        CRYPT_PROVIDER_SGNR* signer = providerData == nullptr ? nullptr : WTHelperGetProvSignerFromChain(providerData, 0, FALSE, 0);
        if (signer == nullptr || signer->csCertChain == 0) {
            log.Info(L"No signer certificate found for %ls", path.c_str());
            return false;
        }
        CRYPT_PROVIDER_CERT* signerCert = WTHelperGetProvCertFromChain(signer, 0);
        CRYPT_PROVIDER_CERT* rootCert = WTHelperGetProvCertFromChain(signer, signer->csCertChain - 1);
        if (signerCert == nullptr || rootCert == nullptr) {
            log.Info(L"Incomplete certificate chain for %ls", path.c_str());
            return false;
        }
        if (!HasEku(signerCert->pCert, signerEku)) {
            log.Info(L"Signing certificate of %ls lacks the extended key usage %hs", path.c_str(), signerEku.c_str());
            return false;
        }
        if (!HasThumbprint(rootCert->pCert, rootThumbprint)) {
            log.Info(L"Certificate chain of %ls ends at an unexpected root", path.c_str());
            return false;
        }
        return true;
    }

}

bool VerifySignature(HANDLE file, const std::wstring& path, const std::string& signerEku, const std::array<BYTE, 20>& rootThumbprint, Log& log) {
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = path.c_str();
    fileInfo.hFile = file;

    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &fileInfo;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;

    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);
    bool valid;
    if (status != ERROR_SUCCESS) {
        log.Error(static_cast<DWORD>(status), L"WinVerifyTrust rejected %ls", path.c_str());
        valid = false;
    } else {
        valid = CheckSigner(data.hWVTStateData, path, signerEku, rootThumbprint, log);
    }

    // release the state data allocated by WTD_STATEACTION_VERIFY
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);
    return valid;
}

#endif
