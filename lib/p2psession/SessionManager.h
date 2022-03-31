#pragma once

#include <string>
#include <string_view>
#include <functional>
#include "WpaEvent.h"
#include "WpaMessages.h"
#include "CoTask.h"
#include "Log.h"
#include <memory>
#include "MessageAwaiter.h"

#ifdef JUNK
namespace p2psession
{

    class SessionManager
    {
        friend class MessageAwaiter;

    public:
        SessionManager();
        virtual ~SessionManager();

        void SetLogLevel(LogLevel level) { log->SetLogLevel(level); }
        LogLevel GetLogLevel() const { return log->GetLogLevel(); }
        void SetLog(std::shared_ptr<ILog> log);


        void Open(const std::string &path);

        void Run();

        void Close();

        // Task methods.
        using ListenerHandle = uint64_t;

        // WpaEvent& event = co_wait CoWaitForMessage(...)
        MessageAwaiter CoWaitForMessage(WpaEventMessage message, int64_t timeoutMs = -1);
        // WpaEvent& event = co_wait CoWaitForMessage(...)
        MessageAwaiter CoWaitForMessages(const std::vector<WpaEventMessage> &messages, int64_t timeoutMs = -1);

        // Listener-based event listening.

        using EventListenerFn = void(const WpaEvent &event);
        ListenerHandle AddEventListener(const std::function<EventListenerFn> &callback, const std::vector<WpaEventMessage> &messages);
        ListenerHandle AddEventListener(const std::function<EventListenerFn> &callback, WpaEventMessage message);
        void RemoveEventListener(ListenerHandle handle);

    private:
        WpaEvent wpaEvent;

        void PostMessageAwaiter(
            MessageAwaiter *pAwaiter);

        std::vector<MessageAwaiter *> messageAwaiters;

        std::shared_ptr<ILog> log;

        void LogDebug(const std::string_view &tag, const std::string_view &message);
        void LogInfo(const std::string_view &tag, const std::string_view &message);
        void LogWarning(const std::string_view &tag, const std::string_view &message);
        void LogError(const std::string_view &tag, const std::string_view &message);
        [[noreturn]] void ThrowError(const std::string &message);

        void ProcessMessages();
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

        struct wpa_ctrl *ctrl = nullptr;
        bool attached = false;
 
    };
}
#endif