#pragma once

#include <string>
#include <iostream>
#include <mutex>

namespace p2psession
{

    enum LogLevel
    {
        Debug,
        Information,
        Warning,
        Error
    };

    class ILog
    {
    public:
        virtual ~ILog() {}

        void SetLogLevel(LogLevel logLevel) { this->logLevel = logLevel; }
        LogLevel GetLogLevel() const { return this->logLevel; }

        void Debug(const std::string &message)
        {
            if (logLevel <= LogLevel::Debug)
            {
                OnDebug(message);
            }
        }
        void Warning(const std::string &message)
        {
            if (logLevel <= LogLevel::Warning)
            {
                OnWarning(message);
            }
        }
        void Info(const std::string &message)
        {
            if (logLevel <= LogLevel::Information)
            {
                OnDebug(message);
            }
        }
        void Error(const std::string &message)
        {
            if (logLevel <= LogLevel::Error)
            {
                OnDebug(message);
            }
        }

    protected:
        virtual void OnDebug(const std::string &message) = 0;
        virtual void OnInformation(const std::string &message) = 0;
        virtual void OnWarning(const std::string &message) = 0;
        virtual void OnError(const std::string &message) = 0;

    private:
        LogLevel logLevel = LogLevel::Warning;
    };

    class ConsoleLog : public ILog
    {
    private:
        std::mutex mutex;

    protected:
        virtual void OnDebug(const std::string &message)
        {
            std::unique_lock lock{mutex};
            std::cout << "dbg:" << message << std::endl;
        }
        virtual void OnInformation(const std::string &message)
        {
            std::unique_lock lock{mutex};
            std::cout << "inf:" << message << std::endl;
        }
        virtual void OnWarning(const std::string &message) 
        {
            std::unique_lock lock{mutex};
            std::cout << "WRN:" << message << std::endl;
        }
        virtual void OnError(const std::string &message) 
                {
            std::unique_lock lock{mutex};
            std::cout << "ERR:" << message << std::endl;
        }

    };

}; // namespace
