#include "utils/Crypto.h"
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QByteArray>
#include <QDebug>
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
        // In production, decrypt here
        return value.toStdString();
    }
    
    return "";
}

void Crypto::setApiKey(const std::string& keyName, const std::string& value) {
    QSettings settings(QString::fromStdString(getConfigPath()), QSettings::IniFormat);
    // In production, encrypt here
    settings.setValue(QString::fromStdString(keyName), QString::fromStdString(value));
    settings.sync();
}

void Crypto::removeApiKey(const std::string& keyName) {
    QSettings settings(QString::fromStdString(getConfigPath()), QSettings::IniFormat);
    settings.remove(QString::fromStdString(keyName));
    settings.sync();
}

std::string Crypto::encrypt(const std::string& plaintext) {
    // TODO: Implement proper encryption (AES-256)
    // For now, return as-is (not secure, but functional)
    return plaintext;
}

std::string Crypto::decrypt(const std::string& ciphertext) {
    // TODO: Implement proper decryption
    return ciphertext;
}

} // namespace ModAI

