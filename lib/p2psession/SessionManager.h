#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <functional>
#include "WpaEvent.h"
#include "WpaMessages.h"
#incldue "CoTask.h"

struct wpa_ctrl;

namespace p2psession
{

    enum class LogLevel
    {
        Debug,
        Information,
        Warning,
        Error
    };

    class SessionManager
    {
    public:
        SessionManager();
        virtual ~SessionManager();

        using LogCallback = void(LogLevel logLevel, const std::string_view &message);
        void SetLogCallback(std::function<LogCallback> callback);

        void Open(const std::string &path);
        void Close();

        LogLevel GetLogLevel() const { return logLevel; }
        void SetLogLevel(LogLevel logLevel) { this->logLevel = logLevel; }

        // Task methods.
        using ListenerHandle = uint64_t;

        Task<WpaEvent &> WaitForMessage(WpaEventMessage message, int64_t timeoutMs = -1);
        Task<WpaEvent &> WaitForMessages(const std::vector<WpaEventMessage> &messages, int64_t timeoutMs = -1);
        void StartTask(Task<> &task);

        // Listener-based event listening.

        using EventListenerFn = void(const WpaEvent &event);
        ListenerHandle AddEventListener(const std::function<EventListenerFn> &callback, const std::vector<WpaEventMessage> &messages);
        ListenerHandle AddEventListener(const std::function<EventListenerFn> &callback, WpaEventMessage message);
        void RemoveEventListener(ListenerHandle handle);

    private:
        WpaEvent wpaEvent;

        LogLevel logLevel = LogLevel::Information;

        std::function<LogCallback> logCallback;

        void Log(LogLevel logLevel, const std::string_view &message);

        void LogDebug(const std::string_view &message);
        void LogDebug(const std::string_view &tag, const std::string_view &message);

        void LogInfo(const std::string_view &message);
        void LogInfo(const std::string_view &tag, const std::string_view &message);
        void LogWarning(const std::string_view &message);
        void LogWarning(const std::string_view &tag, const std::string_view &message);
        void LogError(const std::string_view &message);
        void LogError(const std::string_view &tag, const std::string_view &message);

        void ProcessMessage();
        bool ProcessEvents(const char *line);

        // Listener
        ListenerHandle nextEventListenerHandle = 0x100;
        struct EventListenerEntry
        {
            ListenerHandle handle;
            std::vector<WpaEventMessage> messages;
            std::function<EventListenerFn> callback;
        };
        void FireEvent(const WpaEvent &event);

        std::vector<EventListenerEntry> eventListeners;
        // Task methods.
        class MessageEventSource
        {
        public:
            MessageEventSource(SessionManager &sessionManager)
                : sessionManager(sessionManager)
            {
            }
            EventSource<const WpaEvent*> eventSource;

            SessionManager &sessionManager;

            Task<const WpaEvent*> MessageEvents(WpaEventMessage message)
            {
                while (true)
                {
                    const WpaEvent*event = co_await eventSource.Wait();
                    if (event->message == message)
                    {
                        co_return event;
                    }
                }
            }

            Task<const WpaEvent*> MessageEvents(std::vector<WpaEventMessage> messages)
            {
                while (true)
                {
                    const WpaEvent*event = co_await eventSource.Wait();
                    if (std::find(messages.begin(), messages.end(), event->message) != messages.end())
                    {
                        co_return event;
                    }
                }
            }
        };

        struct wpa_ctrl *ctrl = nullptr;
        bool attached = false;
    };
}