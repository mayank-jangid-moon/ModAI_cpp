#include "utils/Crypto.h"
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QByteArray>
#include <QDebug>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace ModAI {

std::string Crypto::getConfigPath() {
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir().mkpath(configPath);
    return (configPath + "/ModAI/config.ini").toStdString();
}

std::string Crypto::getApiKey(const std::string& keyName) {
    // First check environment variables
    std::string envVar = "MODAI_" + keyName;
    std::transform(envVar.begin(), envVar.end(), envVar.begin(), ::toupper);
    const char* envValue = std::getenv(envVar.c_str());
    if (envValue) {
        return std::string(envValue);
    }
    
    // Then check config file
    QSettings settings(QString::fromStdString(getConfigPath()), QSettings::IniFormat);
    QString value = settings.value(QString::fromStdString(keyName)).toString();
    
    if (!value.isEmpty()) {
        return decrypt(value.toStdString());
    }
    
    return "";
}

void Crypto::setApiKey(const std::string& keyName, const std::string& value) {
    QSettings settings(QString::fromStdString(getConfigPath()), QSettings::IniFormat);
    settings.setValue(QString::fromStdString(keyName), QString::fromStdString(encrypt(value)));
    settings.sync();
}

void Crypto::removeApiKey(const std::string& keyName) {
    QSettings settings(QString::fromStdString(getConfigPath()), QSettings::IniFormat);
    settings.remove(QString::fromStdString(keyName));
    settings.sync();
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

    QByteArray combined;
    combined.append(reinterpret_cast<const char*>(iv.data()), iv.size());
    combined.append(reinterpret_cast<const char*>(ciphertext.data()), total);
    return combined.toBase64().toStdString();
}

std::string Crypto::decrypt(const std::string& ciphertext) {
    if (ciphertext.empty()) return "";

    QByteArray decoded = QByteArray::fromBase64(QByteArray::fromStdString(ciphertext));
    if (decoded.size() <= 16) return "";

    std::vector<unsigned char> key, iv(16);
    deriveKeyAndIv(key, iv);

    const unsigned char* ivPtr = reinterpret_cast<const unsigned char*>(decoded.constData());
    const unsigned char* dataPtr = reinterpret_cast<const unsigned char*>(decoded.constData() + 16);
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

