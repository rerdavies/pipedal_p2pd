#include "p2psession/AsyncIo.h"
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
#include "p2psession/CoService.h"

using namespace p2psession;
using namespace std;

AsyncIo *AsyncIo::instance;


void AsyncIoException::ThrowErrno()
{
    int err = errno;
    char *strError = strerror(err);
    throw AsyncIoException(err, strError);
}

class AsyncIoLinux : AsyncIo
{
public:

    static constexpr int QUEUE_DEPTH = 100;
    AsyncIoLinux()
    {
        if (AsyncIo::instance != nullptr)
        {
            cout << "Error: More than one instance of AsyncIo." << endl;
            cout.flush();
            std::terminate();
        }
        epoll_fd = epoll_create1(0);
        if (epoll_fd < 0)
        {
            cout << "Error: epoll_create1 failed (" << strerror(errno) << ")" << endl;
            cout.flush();
            terminate();
        }

        AsyncIo::instance = this;
        thread = std::make_unique<jthread>([this]() { ThreadProc(); });
    }
    ~AsyncIoLinux()
    {
        terminating = true;
        thread = nullptr; // delete and join the thread.
        AsyncIo::instance = nullptr;
    }

private:
    bool terminating = false;
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

        EventHandle handle = ++nextHandle;
        std::unique_ptr<EpollEvent> event = std::make_unique<EpollEvent>(handle,fileDescriptor,callback);

        struct epoll_event epollEvent;
        epollEvent.data.ptr = event.get();
        epollEvent.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET ;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fileDescriptor, &epollEvent);
        epollEvents.push_back(std::move(event));
        return handle;
    }
    virtual bool UnwatchFile(EventHandle handle) 
    {
        std::unique_lock lock{epollMutex};

        bool found = false;

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

    void ThreadProc()
    {
        try
        {
            int timeout = 0;
            epoll_event events[10];

            while (true)
            {
                if (terminating) break;
                int result = epoll_wait(epoll_fd, events, sizeof(events) / sizeof(events[0]), 500);
                if (result < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    AsyncIoException::ThrowErrno();
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
    AsyncIoException::ThrowErrno();
}


