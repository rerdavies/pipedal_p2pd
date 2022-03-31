#pragma once

#include "AsyncExec.h"
#include "WpaEvent.h"


namespace p2psession {
    class WpaCliWrapper : public AsyncExec  {
        using base = AsyncExec;
    protected:
        virtual CoTask<> CoOnInit() { co_return; }
        virtual void Init() { return; }
        virtual void OnEvent(WpaEvent &event) { return;}
    private:
        bool traceMessages = false;
        CoTask<> ProcessMessages();
        void Execute(const std::vector<std::string> &arguments);
        void Execute(const std::string &deviceName);
    public:

        [[nodiscard ]] CoTask<> Run(int argc, char**argv);
    };
}// namespace