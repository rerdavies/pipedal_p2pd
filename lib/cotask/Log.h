#pragma once

#include <string>
#include <string_view>
#include <iostream>
#include <mutex>

namespace cotask
{

    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error
    };

    class ILog
    {
    public:
        virtual ~ILog() {}

        virtual void SetLogLevel(LogLevel logLevel) { this->logLevel = logLevel; }

        virtual LogLevel GetLogLevel() const { return this->logLevel; }

        void Debug(const std::string &message)
        {
            if (logLevel <= LogLevel::Debug)
            {
                OnDebug(message);
            }
        }
        void Debug(const char*message)
        {
            if (logLevel <= LogLevel::Debug)
            {
                OnDebug(std::string(message));
            }
        }
        void Warning(const std::string &message)
        {
            if (logLevel <= LogLevel::Warning)
            {
                OnWarning(message);
            }
        }
        void Warning(const char*string)
        {
            if (logLevel <= LogLevel::Warning)
            {
                OnWarning(std::string(string));
            }
        }
        void Info(const std::string &message)
        {
            if (logLevel <= LogLevel::Info)
            {
                OnInfo(message);
            }
        }
        void Info(const char*message)
        {
            if (logLevel <= LogLevel::Info)
            {
                OnInfo((std::string)message);
            }
        }
        void Error(const std::string &message)
        {
            if (logLevel <= LogLevel::Error)
            {
                OnError(message);
            }
        }
        void Error(const char*message)
        {
            if (logLevel <= LogLevel::Error)
            {
                OnError((std::string)message);
            }
        }

    protected:
        virtual void OnDebug(const std::string &message) = 0;
        virtual void OnInfo(const std::string &message) = 0;
        virtual void OnWarning(const std::string &message) = 0;
        virtual void OnError(const std::string &message) = 0;

    private:
        LogLevel logLevel = LogLevel::Warning;
    };

    /**
     * @brief Writes log messages to stout.
     * 
     * The default logger.
     * 
     */
    class ConsoleLog : public ILog
    {
    private:
        std::mutex mutex;


    private:
        static std::string now();
    protected:
        virtual void OnDebug(const std::string &message)
        {
            std::lock_guard lock{mutex};
            std::cout << now() << " Debug: " << message << std::endl;
            std::cout.flush();
        }
        virtual void OnInfo(const std::string &message)
        {
            std::lock_guard lock{mutex};
            std::cout << now()<< " Info: " << message << std::endl;
            std::cout.flush();
        }
        virtual void OnWarning(const std::string &message) 
        {
            std::lock_guard lock{mutex};
            std::cout << now() << " Warning: " << message << std::endl;
            std::cout.flush();
        }
        virtual void OnError(const std::string &message) 
                {
            std::lock_guard lock{mutex};
            std::cout << now() << " Error: " << message << std::endl;
            std::cout.flush();
        }
 
    };


#ifdef __linux__
    #include <syslog.h>
    /**
     * @brief Write log messages to the systemd journal. (Linux only)
     * 
     * Must be running under systemd for this to work.
     * 
     * Not that configuration log_level still controls which messages
     * are sent to the systemd journal.
     * 
     */
    class SystemdLog : public ILog
    {


    private:
        std::mutex mutex_;
    protected:
        virtual void OnDebug(const std::string &message)
        {
            std::lock_guard lock {mutex_};
            syslog(LOG_DEBUG, message.c_str());
        }
        virtual void OnInfo(const std::string &message)
        {
            std::lock_guard lock {mutex_};
            syslog(LOG_NOTICE, message.c_str());
        }
        virtual void OnWarning(const std::string &message) 
        {
            std::lock_guard lock {mutex_};
            syslog(LOG_WARNING, message.c_str());
        }
        virtual void OnError(const std::string &message) 
        {
            std::lock_guard lock {mutex_};
            syslog(LOG_ERR, message.c_str());
        }
 
    };
#endif


} // namespace
