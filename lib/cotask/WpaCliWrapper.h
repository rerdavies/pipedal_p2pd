#pragma once

#include "CoExec.h"
#include "CoEvent.h"
#include "WpaEvent.h"


namespace cotask {
    class WpaCliWrapper : private CoExec  {
        using base = CoExec;
    public:
        [[nodiscard ]] CoTask<> Run(int argc, char**argv);

    protected:
        virtual CoTask<> CoOnInit() { co_return; }
        virtual void Init() { return; }
        virtual void OnEvent(WpaEvent &event) { return;}
    private:
        bool traceMessages = true;
        CoTask<> ProcessMessages();
        CoTask<> ReadMessages();
        CoTask<> ReadErrors();
        CoTask<> WriteMessages();

        CoTask<std::vector<std::string>> ReadToPrompt(std::chrono::milliseconds timeout);

        void Execute(const std::vector<std::string> &arguments);
        void Execute(const std::string &deviceName);

        bool lineBufferRecoveringFromTimeout = false;
        int lineBufferHead = 0;
        int lineBufferTail = 0;
        char lineBuffer[512];
        bool lineResultEmpty = true;
        std::stringstream lineResult;

        bool prompting_;
        CoConditionVariable cvPrompting;

        void NotifyPrompting() {
            cvPrompting.Notify([this]() { prompting_ = true;});
        }
        CoTask<> WaitForPrompt(std::chrono::milliseconds timeout) {
            co_await cvPrompting.Wait([this] () {
                if (prompting_)
                {
                    prompting_ = false;
                    return true;
                }
                return false;
            });
            co_return;
        }



    };
}// namespace