#pragma once
#include <string>
#include <vector>

class CryptAPI {
public:
    static std::string Base64Encode(const std::string& input);
    static std::string Base64Decode(const std::string& input);
    static std::string Hash(const std::string& algorithm, const std::string& input);
    static std::string EncryptAES(const std::string& cipher, const std::string& key, const std::string& iv);
    static std::string DecryptAES(const std::string& cipher, const std::string& key, const std::string& iv);
    static std::string GenerateBytes(size_t length);
};
