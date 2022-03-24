#include "p2psession/SessionManager.h"
#include "p2psession/P2pException.h"
#include "p2psession/WpaEvent.h"

#include "ss.h"
#include "stddef.h"
#include "p2psession/wpa_ctrl.h"
#include "ss.h"
#include "stddef.h"
#include <string_view>

using namespace p2psession;
using namespace std;

SessionManager::SessionManager()
{
}

inline void SessionManager::Log(LogLevel logLevel, const std::string_view &message)
{
    this->logCallback(logLevel, message);
}
inline void SessionManager::LogDebug(const std::string_view &message)
{
    if (logLevel <= LogLevel::Debug)
    {
        Log(LogLevel::Debug, message);
    }
}
inline void SessionManager::LogDebug(const std::string_view &tag, const std::string_view &message)
{
    if (logLevel <= LogLevel::Debug)
    {
        string msg = SS(tag << ": " << message);
        Log(LogLevel::Debug, msg);
    }
}

void SessionManager::LogInfo(const std::string_view &message)
{
    if (logLevel <= LogLevel::Information)
    {
        Log(LogLevel::Information, message);
    }
}

inline void SessionManager::LogInfo(const std::string_view &tag, const std::string_view &message)
{
    if (logLevel <= LogLevel::Information)
    {
        string msg = SS(tag << ": " << message);
        Log(LogLevel::Information, msg);
    }
}

void SessionManager::LogWarning(const std::string_view &message)
{
    if (logLevel <= LogLevel::Warning)
        Log(LogLevel::Warning, message);
}

inline void SessionManager::LogWarning(const std::string_view &tag, const std::string_view &message)
{
    if (logLevel <= LogLevel::Warning)
    {
        string msg = SS(tag << ": " << message);
        Log(LogLevel::Warning, msg);
    }
}

void SessionManager::LogError(const std::string_view &message)
{
    if (logLevel <= LogLevel::Error)
        Log(LogLevel::Error, message);
}

inline void SessionManager::LogError(const std::string_view &tag, const std::string_view &message)
{
    if (logLevel <= LogLevel::Error)
    {
        string msg = SS(tag << ": " << message);
        Log(LogLevel::Error, msg);
    }
}

void SessionManager::Open(const std::string &path)
{
    if (ctrl)
    {
        throw P2pException("Already open.");
    }
    ctrl = wpa_ctrl_open(path.c_str());
    if (!ctrl)
    {
        throw P2pException(SS("Can't open " << path));
    }
    int result;
    result = wpa_ctrl_attach(ctrl);
    if (!result)
    {
        if (result == -2)
            throw P2pException("Attach timed out.");
        throw P2pException("Failed to attach.");
    }
    attached = true;
}

void SessionManager::Close()
{
    if (ctrl)
    {
        if (attached)
        {
            wpa_ctrl_detach(ctrl);
            attached = false;
        }
        wpa_ctrl_close(ctrl);
        ctrl = nullptr;
    }
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

void SessionManager::ProcessMessage()
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
            LogDebug(string_view("WpaRead:"), string_view(reply));
            if (result != 0)
            {
                if (result == -2)
                {
                    LogError("Connection timed out.");
                    return;
                }
                else
                {
                    LogError("Connection lost.");
                    return;
                }
            }
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
        LogError(SS("Unexpected error: " << e.what()));
        LogError(string_view("Terminating"));
    }
}

bool SessionManager::ProcessEvents(const char*line)
{
    if (!wpaEvent.ParseLine(line))
    {
        return false;
    }
    if (wpaEvent.message == WpaEventMessage::WPA_UNKOWN_MESSAGE)
    {
        LogDebug("Unknown message recived: ",string_view(wpaEvent.messageString));
    }

    return true;

}

SessionManager::ListenerHandle SessionManager::AddEventListener(const std::function<EventListenerFn> &callback, const std::vector<WpaEventMessage> &messages)
{
    ListenerHandle handle = nextEventListenerHandle++;
    EventListenerEntry entry { handle: handle,messages: messages,callback: callback,   };
    eventListeners.push_back(std::move(entry));
    return handle;
}
SessionManager::ListenerHandle SessionManager::AddEventListener(const std::function<EventListenerFn> &callback, WpaEventMessage message)
{
    return AddEventListener(callback,std::vector<WpaEventMessage> {{ message}});
}
void SessionManager::RemoveEventListener(ListenerHandle handle) {
    for (auto iter = eventListeners.begin(); iter != eventListeners.end(); ++iter)
    {
        if (iter->handle == handle)
        {
            eventListeners.erase(iter);
            return;
        }
    }
}

void SessionManager::FireEvent(const WpaEvent &wpaEvent) {
    std::vector<EventListenerEntry> events;
    // save a copy so clients that add message listeners in response to messages fired don't get spurious events
    events.insert(events.end(),this->eventListeners.begin(),this->eventListeners.end());
    for (auto& event: events)
    {
        bool matches = false;
        for (auto message: event.messages)
        {
            if (message == wpaEvent.message)
            {
                matches = true;
                break;
            }
        }
        event.callback(wpaEvent);
    }
}
