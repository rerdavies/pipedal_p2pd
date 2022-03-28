#include "p2psession/AsyncFile.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "p2psession/CoService.h"
#include <functional>
#include <sys/socket.h>
#include <sys/un.h>

using namespace p2psession;
using namespace p2psession::private_;
using namespace std;

bool AsyncFile::IsReady(WaitOperation op)
{
    switch (op)
    {
    case WaitOperation::Read:
        return readReady;
    case WaitOperation::Write:
        return writeReady;
    default:
        throw std::invalid_argument("Invalid argument.");
    }
}
void AsyncFile::SetReadyCallback(WaitOperation op, WaitInterface::callback callbackOnReady)
{
    std::lock_guard lock { callbackMutex };

    switch (op)
    {
    case WaitOperation::Read:
        this->readCallback = callbackOnReady;
        break;
    case WaitOperation::Write:
        this->writeCallback = callbackOnReady;
        break;
    default:
        throw std::invalid_argument("Invalid argument.");
    }
}
void AsyncFile::ClearReadyCallback(WaitOperation op)
{
    std::lock_guard lock { callbackMutex };

    switch (op)
    {
    case WaitOperation::Read:
        this->readCallback = nullptr;
        break;
    case WaitOperation::Write:
        this->writeCallback = nullptr;
        break;
    default:
        throw std::invalid_argument("Invalid argument.");
    }
}

auto IoWait(WaitOperation op, WaitInterface *waitInterface, std::chrono::milliseconds timeoutMs)
{
    struct Implementation
    {
        using return_type = void;
        using callback_type = CoServiceCallback<void>;

        ~Implementation() {

        }
        // arguments.
        WaitOperation op;
        WaitInterface *waitInterface = nullptr;
        std::chrono::milliseconds timeoutMs;

        callback_type *pCallback = nullptr;
        


        void Execute(callback_type *pCallback)
        {
            this->pCallback = pCallback;
            if (waitInterface->IsReady(op))
            {
                pCallback->SetComplete();
                return;
            }
            waitInterface->SetReadyCallback(op,[this]() {
                this->waitInterface = nullptr;
                this->pCallback->SetComplete();
            });
            if (timeoutMs > 0ms) {
                pCallback->RequestTimeout(timeoutMs);
            }
        }
        bool CancelExecute(callback_type *pCallback)
        {
            if (waitInterface)
            {
                waitInterface->ClearReadyCallback(op);
                waitInterface = nullptr;
            }
            return true;
        }
        AsyncFile *file;
        std::chrono::milliseconds timeout;
    };

    static_assert(IsVoidCoServiceImplementation<Implementation>);

    using Awaitable = CoService<Implementation>;

    Awaitable awaitable{};
    awaitable.op = op;
    awaitable.timeoutMs = timeoutMs;
    awaitable.waitInterface = waitInterface;
    return awaitable;
}

AsyncFile::AsyncFile(int file_fd)
    : file_fd(file_fd)
{
    WatchFile(file_fd);
}

void AsyncFile::WatchFile(int fd)
{
    if (this->eventHandle != 0)
    {
        AsyncIo::GetInstance().UnwatchFile(eventHandle);
        eventHandle = 0;
    }

    if (fd >= 0)
    {
        readReady = true;
        writeReady = true;
        eventHandle = AsyncIo::GetInstance().WatchFile(fd, [this](AsyncIo::EventData eventData) {
            std::lock_guard lock { callbackMutex };
            if (eventData.readReady || eventData.hasError)
            {
                this->readReady = true;
                if (readCallback) { // RACE CONDITION HERE.
                    readCallback();
                    readCallback = nullptr;
                }
            }
            if (eventData.writeReady || eventData.hasError)
            {
                this->writeReady = true;
                if (writeCallback)
                {
                    writeCallback();
                    writeCallback = nullptr;
                }
            }
        });
    }
}

AsyncFile::~AsyncFile()
{
    Close();
}
void AsyncFile::Close()
{
    if (file_fd != -1)
    {
        WatchFile(-1);
        close(file_fd);
        file_fd = -1;
    }
}

void AsyncFile::Attach(int file_fd)
{
    WatchFile(-1);
    this->file_fd = file_fd;
    WatchFile(this->file_fd);
}
int AsyncFile::Detach()
{
    int result = this->file_fd;
    WatchFile(-1);
    this->file_fd = -1;
    return result;
}
Task<> AsyncFile::CoOpen(const std::filesystem::path &path, AsyncFile::OpenMode mode)
{

    int file_fd = -1;
    constexpr int PERMS = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH; // rw-rw-r--
    switch (mode)
    {
    case OpenMode::Read:
        file_fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        break;
    case OpenMode::Create:
        file_fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, PERMS | O_NONBLOCK);
        break;
    case OpenMode::Append:
        file_fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, PERMS | O_NONBLOCK);
        break;
    }
    if (file_fd == -1)
    {
        AsyncIoException::ThrowErrno();
    }
    this->file_fd = file_fd;
    WatchFile(file_fd);
    co_return;
}

Task<int> AsyncFile::CoRead(void *data, size_t length, std::chrono::milliseconds timeout)
{
    int totalRead = 0;

    char *pData = ((char *)data);

    int nRead = 0;

    readReady = false;
    // read until full, or until there is NO MORE!
    while (length > 0)
    {

        int nRead = read(this->file_fd, pData, length);
        if (nRead == 0)
        {
            co_return totalRead;
        } else
        if (nRead < 0)
        {
            if (errno == EAGAIN)
            {
                if (totalRead != 0)
                {
                    co_return totalRead;
                }
                co_await IoWait(WaitOperation::Read,this,timeout);
            }
            else
            {
                AsyncIoException::ThrowErrno();
            }
        }
        else
        {
            totalRead += nRead;
            length -= nRead;
            pData += nRead;
        }
    }
    co_return totalRead;
}

static std::chrono::milliseconds Now()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
}

Task<ssize_t> AsyncFile::CoWrite(const void *data, size_t length, std::chrono::milliseconds timeout)
{
    std::chrono::milliseconds expiryTime = Now() + timeout;
    this->writeReady = false;
    ssize_t totalWritten = 0;
    const char *p = (char *)data;
    while (length != 0)
    {
        ssize_t nWritten = write(this->file_fd, p, length);
        if (nWritten == 0)
        {
            throw AsyncIoException("Write returned zero. Results are *unspecified* (POSIX 1.1)");
        }
        if (nWritten < 0)
        {
            if (errno == EAGAIN)
            {
                co_await IoWait(WaitOperation::Write,this,timeout);
            }
            else
            {
                AsyncIoException::ThrowErrno();
            }
        }
        else
        {
            totalWritten += nWritten;
            length -= nWritten;
            p += nWritten;
        }
    }
    co_return totalWritten;
}

void AsyncFile::CreateSocketPair(AsyncFile &sender, AsyncFile &receiver)
{
    int sv[2];
    int result = socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sv);
    if (result == -1)
    {
        AsyncIoException::ThrowErrno();
    }
    sender.Attach(sv[0]);
    receiver.Attach(sv[1]);
}