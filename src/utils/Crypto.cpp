#include "utils/Crypto.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <algorithm>

namespace ModAI {

std::string Crypto::getConfigPath() {
    return ""; // Not used
}

std::string Crypto::getApiKey(const std::string& keyName) {
    std::string envVar = "MODAI_" + keyName;
    std::transform(envVar.begin(), envVar.end(), envVar.begin(), ::toupper);
    const char* envValue = std::getenv(envVar.c_str());
    if (envValue) {
        return std::string(envValue);
    }
    return "";
}

void Crypto::setApiKey(const std::string& keyName, const std::string& value) {
    // No-op in headless mode
}

void Crypto::removeApiKey(const std::string& keyName) {
    // No-op in headless mode
}

std::string Crypto::sha256(const std::vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    
    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::string Crypto::encrypt(const std::string& plaintext) { return plaintext; }
std::string Crypto::decrypt(const std::string& ciphertext) { return ciphertext; }

} // namespace ModAI

