#include "cotask/CoFile.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "cotask/CoService.h"
#include <functional>
#include <sys/socket.h>
#include <sys/un.h>
#include "cotask/Os.h"
#include "cotask/CoExceptions.h"

using namespace cotask;
using namespace cotask::detail;
using namespace std;

CoFile::CoFile(int file_fd)
    : file_fd(file_fd)
{
    WatchFile(file_fd);
}

void CoFile::WatchFile(int fd)
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
            if (eventData.readReady || eventData.hasError)
            {
                readCv.Notify([this] {
                    this->readReady = true;
                });
            }
            if (eventData.writeReady || eventData.hasError)
            {
                writeCv.Notify([this] {
                    this->writeReady = true;
                });
            }
        });
    }
}

CoFile::~CoFile()
{
    // pump messages until i/o operations have cleared.
    Close();
    // xxx: pump messages until all i/o operations have cleared.

    while (true)
    {
        // run until idle (no waiting for delayed events.)
        CoDispatcher::CurrentDispatcher().PumpMessages();

        // wait until no operations end up in pending state.
        if (closeCv.Test<bool>([this]() {
                return this->pendingOperations == 0;
            }))
        {
            break;
        }
        // wait for delayed events, and try again.
        CoDispatcher::CurrentDispatcher().PumpMessages(true);
    }
    deleted = true;
}

void CoFile::Close()
{
    if (closeCv.Test<bool>([this]() {
        return this->closed || this->closing;
    }))
    {
        return;
    }
    CoClose().GetResult();
}

void CoFile::CheckUseAfterDelete()
    {
        if (this->deleted) 
    {
        Terminate("Use after free!");
    }
}
CoTask<> CoFile::CoClose()
{
    {
        std::lock_guard lock{closeCv.Mutex()};
        CheckUseAfterDelete();
        if (this->closed)
            co_return;
        if (this->closing)
            co_return;
        this->closed = true;
        this->closing = true;
    }

    readCv.NotifyAll([this] {
        this->closed = true; // because memory barrier. Must be readable before fd is read.
    });
    writeCv.NotifyAll([this] {
        this->closed = true; // because memory barrier.  Must be readable before fd is read.
    });
    {
        std::lock_guard lock{closeCv.Mutex()};
        close(this->file_fd);
        WatchFile(-1);
        this->file_fd = -1;
    }



    // pump until suspended tasks go to zero.
    co_await closeCv.Wait([this] {
        return this->pendingOperations == 0;
    });
}

void CoFile::Attach(int file_fd)
{
    std::unique_lock lock{closeCv.Mutex()};
    if (this->file_fd == -1 && file_fd == -1)
    {
        return;
    }
    if (this->file_fd != -1)
        throw logic_error("File is already open.");

    // set O_NON_BLOCKING
    if (file_fd != -1)
    {
        os::SetFileNonBlocking(file_fd, true);
    }
    this->file_fd = file_fd;
    this->closed = false;
    this->closing = false;
    WatchFile(this->file_fd);
}
int CoFile::Detach()
{
    int oldFd = -1;
    {
        std::unique_lock lock{closeCv.Mutex()};
        oldFd = this->file_fd;
        WatchFile(-1);
        this->file_fd = -1;
        this->closed = true;
        os::SetFileNonBlocking(oldFd, false);
    }
    readCv.Notify([this]() {
        this->closed = true; // ensure cross-thread memory barrier.
    });
    writeCv.Notify([this]() {
        this->closed = true; // ensure cross-thread memory barrier.
    });

    return oldFd;
}
CoTask<> CoFile::CoOpen(const std::filesystem::path &path, CoFile::OpenMode mode)
{

    std::unique_lock lock{closeCv.Mutex()};
    CheckUseAfterDelete();

    if (this->file_fd != -1)
        throw logic_error("File is already open.");

    int file_fd = -1;
    constexpr int PERMS = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH; // rw-rw-r--
    switch (mode)
    {
    case OpenMode::ReadWrite:
    {
        file_fd = open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC, PERMS);
    }
    break;
    case OpenMode::Read:
        file_fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        break;
    case OpenMode::Create:
        file_fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, PERMS);
        break;
    case OpenMode::Append:
        file_fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_NONBLOCK, PERMS);
        break;
    }
    if (file_fd == -1)
    {
        CoIoException::ThrowErrno();
    }
    this->file_fd = file_fd;
    this->closed = false;
    this->closing = false;

    WatchFile(file_fd);

    co_return;
}

CoTask<size_t> CoFile::CoRead(void *data, size_t length, std::chrono::milliseconds timeout)
{
    OpsLock opLock(this); // count outstanding iops;

    size_t totalRead = 0;

    char *pData = ((char *)data);

    std::unique_lock lock{readCv.Mutex()};
    CheckUseAfterDelete();

    // read until full, or until there is NO MORE!
    while (length > 0)
    {

        if (closed)
        {
            throw CoIoClosedException();
        }
        ssize_t nRead = read(this->file_fd, pData, length);
        if (nRead == 0)
        {
            co_return totalRead;
        }
        else if (nRead < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // Posix: it's unspecified which we'll get. Both mean the same.
            {
                if (totalRead != 0)
                {
                    co_return totalRead;
                }
                readReady = false;
                lock.unlock();

                co_await readCv.Wait(
                    timeout,
                    [this]() {
                        return this->readReady || this->closed;
                    });
                 
                lock.lock();
            }
            else
            {
                CoIoException::ThrowErrno();
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

CoTask<size_t> CoFile::CoRecv(void *data, size_t length, std::chrono::milliseconds timeout)
{
    OpsLock opLock(this); // count outstanding iops;

    char *pData = ((char *)data);

    std::unique_lock lock{readCv.Mutex()};
    CheckUseAfterDelete();

    while (true)
    {

        if (closed)
        {
            throw CoIoClosedException();
        }
        int nRead = recv(this->file_fd, pData, length, 0);
        //cout << "recv  nRead = " <<nRead << endl;
        if (nRead < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // Posix: it's unspecified which we'll get. Both mean the same.
            {
                readReady = false;
                lock.unlock();

                co_await readCv.Wait(
                    timeout,
                    [this]() {
                        bool ready =  this->readReady || this->closed;
                        //cout << "recv ready: " << ready << endl;
                        return ready;
                    });

                lock.lock();
                continue;
            }
            else
            {
                CoIoException::ThrowErrno();
            }
        }
        co_return nRead;
    }
}

CoTask<> CoFile::CoWrite(const void *data, size_t length, std::chrono::milliseconds timeout)
{
    OpsLock opLock(this); // count outstanding iops;

    std::unique_lock lock{writeCv.Mutex()};
    CheckUseAfterDelete();

    const char *p = (char *)data;
    while (length != 0)
    {
        if (closed)
        {
            throw CoIoClosedException();
        }
        ssize_t nWritten = write(this->file_fd, p, length);
        if (nWritten == 0)
        {
            throw std::logic_error("Write returned zero. Results are *unspecified* (POSIX 1.1). Should you be using CoRecv?");
        }
        if (nWritten < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                lock.unlock();

                co_await writeCv.Wait(
                    timeout,
                    [this]() {
                        return this->writeReady || this->closed;
                    });
                
                lock.lock();
                continue;
            }
            else
            {
                CoIoException::ThrowErrno();
            }
        }
        else
        {
            length -= nWritten;
            p += nWritten;
        }
    }
    co_return;
}
CoTask<> CoFile::CoSend(const void *data, size_t length, std::chrono::milliseconds timeout)
{
    OpsLock opLock(this); // count outstanding iops;

    this->writeReady = false;
    const char *p = (char *)data;

    std::unique_lock lock{writeCv.Mutex()};
    CheckUseAfterDelete();

    while (true) //if the length is zero, it could be a zero-length datagram.
    {
        if (closed)
        {
            throw CoIoClosedException();
        }
        ssize_t nWritten = send(this->file_fd, p, length, 0);
        if (nWritten < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                writeReady = false;
                lock.unlock();

                co_await this->writeCv.Wait(
                    timeout,
                    [this]() {
                        return this->writeReady || this->closed;
                    });
                lock.lock();
            }
            else
            {
                CoIoException::ThrowErrno();
            }
        }
        else
        {
            length -= nWritten;
            p += nWritten;
            if (length == 0)
                break;
        }
    }
    co_return;
}

void CoFile::CreateSocketPair(CoFile &sender, CoFile &receiver)
{
    int sv[2];
    // int result = pipe(sv);
    int result = socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sv);
    if (result == -1)
    {
        CoIoException::ThrowErrno();
    }
    sender.Attach(sv[0]);
    receiver.Attach(sv[1]);
}

CoTask<> CoFile::CoWriteLine(const std::string &line, std::chrono::milliseconds timeout)
{

    co_await CoWrite(line.c_str(), line.length(), timeout);
    char endl = '\n';
    co_await CoWrite(&endl, 1, timeout);
}
CoTask<bool> CoFile::CoReadLine(std::string *result)
{
    while (true)
    {
        while (lineHead != lineTail)
        {
            char c = lineBuffer[lineHead++];
            if (c == '\n')
            {
                *result = lineSS.str();
                lineSS = stringstream();
                co_return true;
            }

            lineSS << c;
        }
        int nRead = co_await CoRead(lineBuffer, sizeof(lineBuffer));
        if (nRead == 0)
        {
            *result = lineSS.str();
            lineSS.clear();
            co_return result->length() != 0;
        }
        lineHead = 0;
        lineTail = nRead;
    }
}