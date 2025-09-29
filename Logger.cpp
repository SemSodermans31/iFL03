#include "Logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>
#include <Windows.h>

Logger& Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

void Logger::init(const std::wstring& filePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_path = filePath;

    // Ensure directory exists
    if (!m_path.empty())
    {
        try
        {
            std::filesystem::path p(m_path);
            std::filesystem::create_directories(p.parent_path());
        }
        catch (...)
        {
            // Best effort; failure will surface when opening the stream
        }
    }

    m_initialized = true;
    ensureOpenUnlocked();
}

bool Logger::ensureOpenUnlocked()
{
    if (!m_initialized)
        return false;

    if (!m_stream.is_open())
    {
        m_stream.open(m_path, std::ios::out | std::ios::app);
    }
    return m_stream.is_open();
}

void Logger::log(LogLevel level, const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!ensureOpenUnlocked())
        return;

    writeLineUnlocked(level, message);
}

void Logger::log(LogLevel level, const std::wstring& message)
{
    log(level, narrow(message));
}

void Logger::logInfo(const std::string& message)
{
    log(LogLevel::Info, message);
}

void Logger::logWarning(const std::string& message)
{
    log(LogLevel::Warning, message);
}

void Logger::logError(const std::string& message)
{
    log(LogLevel::Error, message);
}

void Logger::writeLineUnlocked(LogLevel level, const std::string& message)
{
    if (!m_stream.is_open())
        return;

    m_stream << makeTimestamp() << " [" << levelToString(level) << "] " << message << std::endl;
}

void Logger::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stream.is_open())
        m_stream.flush();
}

std::string Logger::levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Info: return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error: return "ERROR";
    default: return "LOG";
    }
}

std::string Logger::makeTimestamp()
{
    using namespace std::chrono;

    auto now = system_clock::now();
    auto timeT = system_clock::to_time_t(now);
    auto tm = std::tm{};
    localtime_s(&tm, &timeT);

    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

std::string Logger::narrow(const std::wstring& wide)
{
    if (wide.empty())
        return {};

    int required = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (required <= 0)
        return {};

    std::vector<char> buffer(required);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), buffer.data(), required, nullptr, nullptr);
    return std::string(buffer.begin(), buffer.end());
}


