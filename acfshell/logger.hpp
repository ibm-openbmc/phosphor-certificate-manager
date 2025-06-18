#pragma once
#include <format>
#include <iostream>
#include <string>
namespace scrrunner
{
enum class LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR
};
template <typename OutputStream>
class Logger
{
  public:
    Logger(LogLevel level, OutputStream& outputStream) :
        currentLogLevel(level), output(outputStream)
    {}

    void log(const char* filename, int lineNumber, LogLevel level,
             const std::string& message) const
    {
        if (isLogLevelEnabled(level))
        {
            output << std::format("{}:{} ", filename, lineNumber) << message
                   << "\n";
        }
    }

    void setLogLevel(LogLevel level)
    {
        currentLogLevel = level;
    }

  private:
    LogLevel currentLogLevel;
    OutputStream& output;

    bool isLogLevelEnabled(LogLevel level) const
    {
        return level >= currentLogLevel;
    }
};

inline Logger<std::ostream>& getLogger()
{
    static Logger<std::ostream> logger(LogLevel::ERROR, std::cout);
    return logger;
}
} // namespace scrrunner

// Macros for clients to use logger
#define LOG_DEBUG(message, ...)                                                \
    scrrunner::getLogger().log(                                                \
        __FILE__, __LINE__, scrrunner::LogLevel::DEBUG,                        \
        std::format("{} :" message, "Debug", ##__VA_ARGS__))
#define LOG_INFO(message, ...)                                                 \
    scrrunner::getLogger().log(                                                \
        __FILE__, __LINE__, scrrunner::LogLevel::INFO,                         \
        std::format("{} :" message, "Info", ##__VA_ARGS__))
#define LOG_WARNING(message, ...)                                              \
    scrrunner::getLogger().log(                                                \
        __FILE__, __LINE__, scrrunner::LogLevel::WARNING,                      \
        std::format("{} :" message, "Warning", ##__VA_ARGS__))
#define LOG_ERROR(message, ...)                                                \
    scrrunner::getLogger().log(                                                \
        __FILE__, __LINE__, scrrunner::LogLevel::ERROR,                        \
        std::format("{} :" message, "Error", ##__VA_ARGS__))

#define CLIENT_LOG_DEBUG(message, ...) LOG_DEBUG(message, ##__VA_ARGS__)
#define CLIENT_LOG_INFO(message, ...) LOG_INFO(message, ##__VA_ARGS__)
#define CLIENT_LOG_WARNING(message, ...) LOG_WARNING(message, ##__VA_ARGS__)
#define CLIENT_LOG_ERROR(message, ...) LOG_ERROR(message, ##__VA_ARGS__)
