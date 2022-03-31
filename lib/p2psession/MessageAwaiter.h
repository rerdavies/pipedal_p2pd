
// included in SessionManager.h. Don't include directly.
#pragma once
#include "CoExceptions.h"

namespace p2psession
{
    // private implementation.
    // implementation is in SessionManager.cpp.

    class SessionManager;

    struct MessageAwaiter
    {
        ~MessageAwaiter();
        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) noexcept;

        const WpaEvent &await_resume() const;

    private:
        friend class SessionManager;
        std::coroutine_handle<> parentHandle;
        std::vector<WpaEventMessage> messages;
        int64_t timeoutMs;
        const WpaEvent *wpaEvent = nullptr;

        SessionManager *sessionManager = nullptr;
        bool timedOut = false;
        bool cancelled = false;
    };
} // namespace
