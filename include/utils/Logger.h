#pragma once

#include <string>

namespace ModAI {

class Logger {
public:
    enum Level {
        Debug,
        Info,
        Warn,
        Error
    };
    
    static void init(const std::string& logFile);
    static void log(Level level, const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);
    
private:
    static std::string logFile_;
    static std::string levelToString(Level level);
};

} // namespace ModAI

