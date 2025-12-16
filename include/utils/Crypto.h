#pragma once

#include <string>
#include <vector>

namespace ModAI {

class Crypto {
public:
    static std::string getApiKey(const std::string& keyName);
    static void setApiKey(const std::string& keyName, const std::string& value);
    static void removeApiKey(const std::string& keyName);
    static std::string sha256(const std::vector<uint8_t>& data);
    
private:
    static std::string getConfigPath();
    static std::string encrypt(const std::string& plaintext);
    static std::string decrypt(const std::string& ciphertext);
};

} // namespace ModAI

