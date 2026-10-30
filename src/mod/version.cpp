#include "version.h"

#include <windows.h>
#include <bcrypt.h>

#include <vector>

namespace {

// SHA-256 of the Steam Bejeweled3.exe (PE timestamp 2011-10-25), the build
// all of docs/re was done on (docs/research.md).
constexpr const char* kSupportedSha256 =
    "d55162445511fcbf711ab7922e57d9bdc27ba2fa72b3bde6ab553e0756e688c3";

bool Sha256File(const wchar_t* path, std::string* hex) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
              BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0;
    std::vector<unsigned char> buf(1 << 16);
    DWORD got = 0;
    while (ok && ReadFile(f, buf.data(), (DWORD)buf.size(), &got, nullptr) && got > 0)
        ok = BCryptHashData(hash, buf.data(), got, 0) == 0;
    unsigned char digest[32];
    ok = ok && BCryptFinishHash(hash, digest, sizeof digest, 0) == 0;
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    if (!ok) return false;
    static const char digits[] = "0123456789abcdef";
    hex->clear();
    for (unsigned char b : digest) {
        hex->push_back(digits[b >> 4]);
        hex->push_back(digits[b & 15]);
    }
    return true;
}

}  // namespace

bool GameBuildSupported(std::string* hashHex) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
    if (!Sha256File(path, hashHex)) return false;
    return *hashHex == kSupportedSha256;
}
