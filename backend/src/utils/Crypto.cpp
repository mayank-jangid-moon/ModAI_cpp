#include "utils/Crypto.h"
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>
#include <map>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

namespace fs = std::filesystem;

namespace ModAI {

std::string base64_encode(const unsigned char* buffer, size_t length) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return result;
}

std::vector<unsigned char> base64_decode(const std::string& encoded) {
    BIO *bio, *b64;
    std::vector<unsigned char> decoded(encoded.size());

    bio = BIO_new_mem_buf(encoded.data(), encoded.length());
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    int decodedLength = BIO_read(bio, decoded.data(), encoded.length());
    BIO_free_all(bio);

    if (decodedLength > 0) {
        decoded.resize(decodedLength);
    } else {
        decoded.clear();
    }

    return decoded;
}

std::string Crypto::getConfigPath() {
    // Use $HOME/.config/ModAI/config.ini or fallback to /tmp
    const char* homeDir = std::getenv("HOME");
    if (!homeDir) {
        homeDir = "/tmp";
    }
    
    std::string configDir = std::string(homeDir) + "/.config/ModAI";
    fs::create_directories(configDir);
    return configDir + "/config.ini";
}

std::string Crypto::getApiKey(const std::string& keyName) {
    // First check environment variables
    std::string envVar = "MODAI_" + keyName;
    std::transform(envVar.begin(), envVar.end(), envVar.begin(), ::toupper);
    const char* envValue = std::getenv(envVar.c_str());
    if (envValue) {
        return std::string(envValue);
    }
    
    // Then check config file (simple INI-style format)
    std::string configPath = getConfigPath();
    std::ifstream file(configPath);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                
                if (key == keyName && !value.empty()) {
                    return decrypt(value);
                }
            }
        }
    }
    
    return "";
}

void Crypto::setApiKey(const std::string& keyName, const std::string& value) {
    std::string configPath = getConfigPath();
    std::map<std::string, std::string> config;
    
    // Read existing config
    std::ifstream inFile(configPath);
    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string val = line.substr(pos + 1);
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                val.erase(0, val.find_first_not_of(" \t\r\n"));
                val.erase(val.find_last_not_of(" \t\r\n") + 1);
                config[key] = val;
            }
        }
        inFile.close();
    }
    
    // Update the key
    config[keyName] = encrypt(value);
    
    // Write back
    std::ofstream outFile(configPath);
    if (outFile.is_open()) {
        for (const auto& [k, v] : config) {
            outFile << k << "=" << v << "\n";
        }
        outFile.close();
    }
}

void Crypto::removeApiKey(const std::string& keyName) {
    std::string configPath = getConfigPath();
    std::map<std::string, std::string> config;
    
    // Read existing config
    std::ifstream inFile(configPath);
    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string val = line.substr(pos + 1);
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                val.erase(0, val.find_first_not_of(" \t\r\n"));
                val.erase(val.find_last_not_of(" \t\r\n") + 1);
                if (key != keyName) {
                    config[key] = val;
                }
            }
        }
        inFile.close();
    }
    
    // Write back
    std::ofstream outFile(configPath);
    if (outFile.is_open()) {
        for (const auto& [k, v] : config) {
            outFile << k << "=" << v << "\n";
        }
        outFile.close();
    }
}

static void deriveKeyAndIv(std::vector<unsigned char>& key, std::vector<unsigned char>& iv) {
    std::string passphrase = std::getenv("MODAI_PASSPHRASE") ? std::getenv("MODAI_PASSPHRASE") : "modai-default-passphrase";
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(passphrase.data()), passphrase.size(), hash);
    key.assign(hash, hash + 32);

    SHA256(reinterpret_cast<const unsigned char*>("modai-iv"), 7, hash);
    iv.assign(hash, hash + 16);
}

std::string Crypto::encrypt(const std::string& plaintext) {
    if (plaintext.empty()) return "";

    std::vector<unsigned char> key, iv;
    deriveKeyAndIv(key, iv);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return plaintext;

    std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    int len = 0, total = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return plaintext;
    }

    if (EVP_EncryptUpdate(ctx,
                          ciphertext.data(),
                          &len,
                          reinterpret_cast<const unsigned char*>(plaintext.data()),
                          plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return plaintext;
    }
    total = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return plaintext;
    }
    total += len;
    EVP_CIPHER_CTX_free(ctx);

    // Combine IV and ciphertext
    std::vector<unsigned char> combined;
    combined.insert(combined.end(), iv.begin(), iv.end());
    combined.insert(combined.end(), ciphertext.begin(), ciphertext.begin() + total);
    
    return base64_encode(combined.data(), combined.size());
}

std::string Crypto::decrypt(const std::string& ciphertext) {
    if (ciphertext.empty()) return "";

    std::vector<unsigned char> decoded = base64_decode(ciphertext);
    if (decoded.size() <= 16) return "";

    std::vector<unsigned char> key, iv(16);
    deriveKeyAndIv(key, iv);

    const unsigned char* ivPtr = decoded.data();
    const unsigned char* dataPtr = decoded.data() + 16;
    int dataLen = decoded.size() - 16;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";

    std::vector<unsigned char> plaintext(dataLen + EVP_MAX_BLOCK_LENGTH);
    int len = 0, total = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), ivPtr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, dataPtr, dataLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    total = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    total += len;
    EVP_CIPHER_CTX_free(ctx);

    return std::string(reinterpret_cast<char*>(plaintext.data()), total);
}

} // namespace ModAI
