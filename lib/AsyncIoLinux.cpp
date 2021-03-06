/*
 * MIT License
 * 
 * Copyright (c) 2022 Robin E. R. Davies
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "cotask/AsyncIo.h"
#include <iostream>
#include <thread>
#include <functional>
#include <memory>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <sys/ioctl.h>
#include "cotask/CoService.h"
#include "ss.h"

using namespace cotask;
using namespace std;

AsyncIo *AsyncIo::instance;


void CoIoException::ThrowErrno()
{
    int err = errno;
    char *strError = strerror(err);
    throw CoIoException(err, strError);
}

class AsyncIoLinux : AsyncIo
{
public:

    static constexpr int QUEUE_DEPTH = 100;
    AsyncIoLinux()
    {
        if (AsyncIo::instance != nullptr)
        {
            Terminate("Error: More than one instance of AsyncIo.");
        }
        epoll_fd = epoll_create1(0);
        if (epoll_fd < 0)
        {
            Terminate(SS("Error: epoll_create1 failed (" << strerror(errno) << ")"));
        }

        AsyncIo::instance = this;
    }
    ~AsyncIoLinux()
    {
        ssource.request_stop();
        thread = nullptr; // delete and join the thread.
        AsyncIo::instance = nullptr;
    }

    virtual void Start()
    {
        if (!thread)
        {
            thread = std::make_unique<jthread>(
                [this, stoken=ssource.get_token()]() { ThreadProc(stoken); });
        }
    }
    virtual void Stop()
    {
        ssource.request_stop();
        thread = nullptr;
    }


private:
    std::stop_source ssource;

    int epoll_fd = -1;

    class EpollEvent
    {
    public:
        EpollEvent() { }
        EpollEvent(EventHandle eventHandle, int fd,EventCallback callback) : handle(eventHandle),fd(fd), callback(callback) {}

        EventHandle handle = -1;
        int fd = -1;
        EventCallback callback = nullptr;
    };
    std::mutex epollMutex;
    std::vector<std::unique_ptr<EpollEvent>> epollEvents;

    EventHandle nextHandle = 0;

    virtual EventHandle WatchFile(int fileDescriptor, EventCallback callback) 
    {
        std::unique_lock lock{epollMutex};
        Start();

        EventHandle handle = ++nextHandle;
        std::unique_ptr<EpollEvent> event = std::make_unique<EpollEvent>(handle,fileDescriptor,callback);

        struct epoll_event epollEvent;
        memset(&epollEvent,0,sizeof(epollEvent));
        epollEvent.data.ptr = event.get();
        epollEvent.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET ;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fileDescriptor, &epollEvent);
        epollEvents.push_back(std::move(event));
        return handle;
    }
    virtual bool UnwatchFile(EventHandle handle) 
    {
        std::unique_lock lock{epollMutex};


        for (auto i = epollEvents.begin(); i != epollEvents.end(); ++i)
        {
            if ((*i)->handle == handle)
            {
                std::unique_ptr event = std::move(*i);
                epollEvents.erase(i); // also destructs the unique_ptr.

                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event->fd, nullptr);
                return true;
            }
        }
        return false;
    }

    void ThreadProc(const std::stop_token& stopToken)
    {
        try
        {
            epoll_event events[10];

            while (true)
            {
                if (stopToken.stop_requested())
                {
                    break;
                }
                int result = epoll_wait(epoll_fd, events, sizeof(events) / sizeof(events[0]), 500);
                if (result < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    CoIoException::ThrowErrno();
                }
                for (int i = 0; i < result; ++i)
                {
                    EpollEvent *ev = (EpollEvent *)events[i].data.ptr;
                    auto flags = events[i].events;
                    EventData eventData;
                    //EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP 
                    eventData.readReady = (flags & EPOLLIN) != 0;
                    eventData.writeReady = (flags & EPOLLOUT) != 0;
                    eventData.hasError = (flags & EPOLLERR) != 0;
                    eventData.hup = (flags & EPOLLHUP) != 0;
                    ev->callback(eventData);
                }
            }
            if (epoll_fd != -1)
            {
                close(epoll_fd);
                epoll_fd = -1;
            }
        }
        catch (std::exception &e)
        {
            cout << "Error: AsyncIo thread terminated unexpectedly (" << e.what() << ")" << endl;
            cout.flush();
            terminate();
        }
    }

    std::unique_ptr<jthread> thread;
};

AsyncIoLinux linuxInstance;

[[noreturn]] void ThrowErrno()
{
    CoIoException::ThrowErrno();
}


