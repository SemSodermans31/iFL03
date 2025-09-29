#pragma once

#include <mutex>
#include <fstream>
#include <string>

enum class LogLevel
{
    Info,
    Warning,
    Error
};

class Logger
{
public:
    static Logger& instance();

    void init(const std::wstring& filePath);

    void log(LogLevel level, const std::string& message);
    void log(LogLevel level, const std::wstring& message);

    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);

    void flush();

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    bool ensureOpenUnlocked();
    void writeLineUnlocked(LogLevel level, const std::string& message);
    static std::string levelToString(LogLevel level);
    static std::string makeTimestamp();
    static std::string narrow(const std::wstring& wide);

    std::mutex      m_mutex;
    std::ofstream   m_stream;
    std::wstring    m_path;
    bool            m_initialized = false;
};


