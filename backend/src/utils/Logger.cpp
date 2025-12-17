#include "utils/Logger.h"
#include <fstream>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace ModAI {

std::string Logger::logFile_;

void Logger::init(const std::string& logFile) {
    logFile_ = logFile;
}

std::string Logger::levelToString(Level level) {
    switch (level) {
        case Debug: return "DEBUG";
        case Info: return "INFO";
        case Warn: return "WARN";
        case Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::log(Level level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    std::string timestamp = ss.str();
    std::string levelStr = levelToString(level);
    std::string logLine = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    // Console output
    std::cout << logLine << std::endl;
    
    // File output
    if (!logFile_.empty()) {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);
        
        std::ofstream file(logFile_, std::ios::app);
        if (file.is_open()) {
            file << logLine << std::endl;
            file.close();
        }
    }
}

void Logger::debug(const std::string& message) {
    log(Debug, message);
}

void Logger::info(const std::string& message) {
    log(Info, message);
}

void Logger::warn(const std::string& message) {
    log(Warn, message);
}

void Logger::error(const std::string& message) {
    log(Error, message);
}

} // namespace ModAI
