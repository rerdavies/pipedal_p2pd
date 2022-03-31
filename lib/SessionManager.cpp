#include "p2psession/SessionManager.h"
#include "p2psession/CoExceptions.h"
#include "p2psession/WpaEvent.h"

#include "ss.h"
#include "stddef.h"
#include "ss.h"
#include "stddef.h"
#include <string_view>

using namespace p2psession;
using namespace std;

#ifdef JUNK
SessionManager::SessionManager()
{
    this->log = std::make_shared<ConsoleLog>();
}
void SessionManager::SetLog(std::shared_ptr<ILog> log)
{
    this->log = log;
}

inline void SessionManager::LogDebug(const std::string_view &tag, const std::string_view &message)
{
    if (GetLogLevel() <= LogLevel::Debug)
    {
        string msg = SS(tag << ": " << message);
        log->Debug(msg);
    }
}

inline void SessionManager::LogInfo(const std::string_view &tag, const std::string_view &message)
{
    if (GetLogLevel() <= LogLevel::Info)
    {
        string msg = SS(tag << ": " << message);
        log->Info(msg);
    }
}

inline void SessionManager::LogWarning(const std::string_view &tag, const std::string_view &message)
{
    if (GetLogLevel() <= LogLevel::Warning)
    {
        string msg = SS(tag << ": " << message);
        log->Warning(msg);
    }
}

inline void SessionManager::LogError(const std::string_view &tag, const std::string_view &message)
{
    if (GetLogLevel() <= LogLevel::Error)
    {
        string msg = SS(tag << ": " << message);
        log->Error(msg);
    }
}

[[noreturn]] void SessionManager::ThrowError(const std::string &message)
{
    log->Error(message);
    throw P2pException(message);
}
void SessionManager::Open(const std::string &path)
{
    attached = true;
}

void SessionManager::Close()
{
}

SessionManager::~SessionManager()
{
    Close();
}

static void tokenize(const char *line, char splitChar, std::vector<std::string_view> &tokens)
{
    tokens.resize(0);
    vector<string_view> result;

    const char *pStart = line;
    while (*pStart)
    {
        const char *pEnd = pStart;
        while (*pEnd != splitChar && *pEnd != '\0')
        {
            ++pEnd;
        }
        if (pEnd != pStart)
        {
            tokens.push_back(std::string_view(pStart, pEnd - pStart));
        }
        if (*pEnd == '-')
            ++pEnd;
        pStart = pEnd;
    }
}

void SessionManager::Run() {
    ProcessMessages();
}
void SessionManager::ProcessMessages()
{
    vector<string_view> tokens;
    try
    {
        while (true)
        {
            char reply[512];
            size_t size;
            int result = wpa_ctrl_recv(ctrl, reply, &size);
            reply[511] = 0;
            if (result <= 0)
            {
                if (errno == EAGAIN)
                {
                    continue;
                }
                if (result == -2)
                {
                    log->Error("wpa_supplicant: Connection timed out.");
                    return;
                }
                else
                {
                    log->Error("wpa_supplicant: Connection lost.");
                    return;
                }
            }
            LogDebug(string_view("WpaRead: "), string_view(reply));
            if (ProcessEvents(reply))
            {
                continue;
            }
        }
    }
    catch (const P2pCancelledException &e)
    {
        return;
    }
    catch (const std::exception e)
    {
        log->Error(SS("Unexpected error: " << e.what()));
        log->Error("Terminating");
    }
}

bool SessionManager::ProcessEvents(const char *line)
{
    if (!wpaEvent.ParseLine(line))
    {
        return false;
    }
    if (wpaEvent.message == WpaEventMessage::WPA_UNKOWN_MESSAGE)
    {
        LogDebug("Unknown message recived: ", string_view(wpaEvent.messageString));
    }

    return true;
}

SessionManager::ListenerHandle SessionManager::AddEventListener(const std::function<EventListenerFn> &callback, const std::vector<WpaEventMessage> &messages)
{
    ListenerHandle handle = nextEventListenerHandle++;
    EventListenerEntry entry{
        handle : handle,
        messages : messages,
        callback : callback,
    };
    eventListeners.push_back(std::move(entry));
    return handle;
}
SessionManager::ListenerHandle SessionManager::AddEventListener(const std::function<EventListenerFn> &callback, WpaEventMessage message)
{
    return AddEventListener(callback, std::vector<WpaEventMessage>{{message}});
}
void SessionManager::RemoveEventListener(ListenerHandle handle)
{
    for (auto iter = eventListeners.begin(); iter != eventListeners.end(); ++iter)
    {
        if (iter->handle == handle)
        {
            eventListeners.erase(iter);
            return;
        }
    }
}

static bool EventMatches(WpaEventMessage message, const std::vector<WpaEventMessage>& messages)
{
    for (size_t i = 0; i < messages.size(); ++i)
    {
        if (messages[i] == message) return true;
    }
    return false;
}

void SessionManager::FireEvent(const WpaEvent &wpaEvent)
{
    auto message = wpaEvent.message;
    size_t count = this->eventListeners.size();
    bool matched = false;
    for (size_t i = 0; i < count; ++i)
    {
        auto &entry = this->eventListeners[i];
        if (EventMatches(message,entry.messages))
        {
            matched = true;
            entry.callback(wpaEvent);
        }
    }
    // now compact out the entries we called.
    if (matched)
    {
        int writeIndex = 0;
        for (size_t i = 0; i < count; ++i)
        {
            auto &entry = this->eventListeners[i];
            if (!EventMatches(message,entry.messages))
            {
                if (i != writeIndex)
                {
                    this->eventListeners[writeIndex++] = std::move(this->eventListeners[i]);
                }
            }
        }
        if (writeIndex != count)
        {
            for (size_t i = count; i < this->eventListeners.size(); ++i)
            {
                this->eventListeners[writeIndex++] = std::move(this->eventListeners[i]);
            }
        }
        this->eventListeners.resize(writeIndex);
    }
    count = messageAwaiters.size();
    matched = false;
    CoDispatcher &dispatcher = CoDispatcher::CurrentDispatcher();
    for (int i = 0; i < count; ++i)
    {
        auto &entry = messageAwaiters[i];
        if (EventMatches(message,entry->messages))
        {
            entry->parentHandle.resume();
            matched = true;
        }
    }
    if (matched)
    {
        size_t writeIndex = 0;
        for (size_t i = 0; i < count; ++i)
        {
            auto & entry = messageAwaiters[i];
            if (!EventMatches(message,entry->messages))
            {
                messageAwaiters[writeIndex++] = (messageAwaiters[i]);
            }
        }
        for (size_t i = 0; i < messageAwaiters.size(); ++i)
        {
            messageAwaiters[writeIndex++] = (messageAwaiters[i]);
        }
        messageAwaiters.resize(writeIndex);
    }
}

void MessageAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
{
    sessionManager->PostMessageAwaiter(this);
}

const WpaEvent &MessageAwaiter::await_resume() const
{
    if (cancelled)
    {
        throw P2pCancelledException();
    }
    if (timedOut)
    {
        throw P2pTimeoutException();
    }
    auto result = wpaEvent;
    // delete this? I think we're on the stack/coroutine-frame, so it's ok. Check.
    return *result;
}

MessageAwaiter::~MessageAwaiter()
{
}

void SessionManager::PostMessageAwaiter(
    MessageAwaiter *pAwaiter)
{
    messageAwaiters.push_back(pAwaiter);
}

MessageAwaiter SessionManager::CoWaitForMessages(const std::vector<WpaEventMessage> &messages, int64_t timeoutMs)
{
    MessageAwaiter result;
    result.wpaEvent = &this->wpaEvent;
    result.messages = std::move(messages);
    result.timeoutMs = timeoutMs;
    result.sessionManager = this;
    return result;
}

MessageAwaiter SessionManager::CoWaitForMessage(WpaEventMessage message, int64_t timeoutMs)
{
    std::vector<WpaEventMessage> messages;
    return CoWaitForMessages(messages, timeoutMs);
}
#endif