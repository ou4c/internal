#include "crypt.h"
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

static const char B64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string CryptAPI::Base64Encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(B64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(B64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

std::string CryptAPI::Base64Decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++)
        T[B64_CHARS[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string CryptAPI::Hash(const std::string& algorithm, const std::string& input) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    LPCWSTR algId = BCRYPT_SHA256_ALGORITHM;

    std::string lowerAlg = algorithm;
    std::transform(lowerAlg.begin(), lowerAlg.end(), lowerAlg.begin(), ::tolower);

    if (lowerAlg == "sha1" || lowerAlg == "sha-1") algId = BCRYPT_SHA1_ALGORITHM;
    else if (lowerAlg == "sha256" || lowerAlg == "sha-256") algId = BCRYPT_SHA256_ALGORITHM;
    else if (lowerAlg == "sha384" || lowerAlg == "sha-384") algId = BCRYPT_SHA384_ALGORITHM;
    else if (lowerAlg == "sha512" || lowerAlg == "sha-512") algId = BCRYPT_SHA512_ALGORITHM;
    else if (lowerAlg == "md5") algId = BCRYPT_MD5_ALGORITHM;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, algId, NULL, 0)))
        return "";

    DWORD cbHash = 0, cbData = 0;
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);

    std::vector<BYTE> hash(cbHash);
    BCRYPT_HASH_HANDLE hHash = NULL;
    if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0))) {
        BCryptHashData(hHash, (PBYTE)input.data(), (ULONG)input.size(), 0);
        BCryptFinishHash(hHash, hash.data(), cbHash, 0);
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);

    std::stringstream ss;
    for (BYTE b : hash)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return ss.str();
}

std::string CryptAPI::EncryptAES(const std::string& plaintext, const std::string& key, const std::string& iv) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0)))
        return "";

    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);

    BCRYPT_KEY_HANDLE hKey = NULL;
    BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, (PBYTE)key.data(), (ULONG)key.size(), 0);

    std::vector<BYTE> ivCopy(iv.begin(), iv.end());
    ivCopy.resize(16, 0);

    DWORD cbCipherText = 0;
    BCryptEncrypt(hKey, (PBYTE)plaintext.data(), (ULONG)plaintext.size(), NULL, ivCopy.data(), 16, NULL, 0, &cbCipherText, BCRYPT_BLOCK_PADDING);

    std::vector<BYTE> cipherText(cbCipherText);
    BCryptEncrypt(hKey, (PBYTE)plaintext.data(), (ULONG)plaintext.size(), NULL, ivCopy.data(), 16, cipherText.data(), cbCipherText, &cbCipherText, BCRYPT_BLOCK_PADDING);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return std::string(cipherText.begin(), cipherText.end());
}

std::string CryptAPI::DecryptAES(const std::string& cipher, const std::string& key, const std::string& iv) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0)))
        return "";

    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);

    BCRYPT_KEY_HANDLE hKey = NULL;
    BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, (PBYTE)key.data(), (ULONG)key.size(), 0);

    std::vector<BYTE> ivCopy(iv.begin(), iv.end());
    ivCopy.resize(16, 0);

    DWORD cbPlainText = 0;
    BCryptDecrypt(hKey, (PBYTE)cipher.data(), (ULONG)cipher.size(), NULL, ivCopy.data(), 16, NULL, 0, &cbPlainText, BCRYPT_BLOCK_PADDING);

    std::vector<BYTE> plainText(cbPlainText);
    BCryptDecrypt(hKey, (PBYTE)cipher.data(), (ULONG)cipher.size(), NULL, ivCopy.data(), 16, plainText.data(), cbPlainText, &cbPlainText, BCRYPT_BLOCK_PADDING);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return std::string(plainText.begin(), plainText.end());
}

std::string CryptAPI::GenerateBytes(size_t length) {
    std::string out(length, 0);
    BCryptGenRandom(NULL, (PBYTE)out.data(), (ULONG)length, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return out;
}
