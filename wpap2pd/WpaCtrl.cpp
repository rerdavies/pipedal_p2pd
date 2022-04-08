#include "includes/WpaCtrl.h"
#include <stdint.h>
#include <stddef.h>
#include <stdexcept>
#include "includes/WpaExceptions.h"
#include "ss.h"
#include <atomic>
#include "includes/P2pUtil.h"

using namespace p2p;
using namespace std;
using namespace p2p::detail;

// PLATFORM INCLUDES
 
#ifndef WIN32
#ifndef CONFIG_CTRL_IFACE_UNIX
#define CONFIG_CTRL_IFACE_UNIX
#endif
#endif

#ifdef CONFIG_CTRL_IFACE_UNIX
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#endif /* CONFIG_CTRL_IFACE_UNIX */
#ifdef CONFIG_CTRL_IFACE_UDP_REMOTE
#include <netdb.h>
#endif /* CONFIG_CTRL_IFACE_UDP_REMOTE */

#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
#define CTRL_IFACE_SOCKET



#endif /* CONFIG_CTRL_IFACE_UNIX || CONFIG_CTRL_IFACE_UDP */

#if defined(CONFIG_CTRL_IFACE_UNIX)
#    ifndef CONFIG_CTRL_IFACE_CLIENT_PREFIX
#        define CONFIG_CTRL_IFACE_CLIENT_DIR "/tmp"
#    endif /* CONFIG_CTRL_IFACE_CLIENT_DIR */
#    ifndef CONFIG_CTRL_IFACE_CLIENT_PREFIX
#        define CONFIG_CTRL_IFACE_CLIENT_PREFIX "wpa_ctrl_"
#    endif /* CONFIG_CTRL_IFACE_CLIENT_PREFIX */
#endif




#ifndef NULL
#define NULL nullptr
#endif

using namespace p2p;
using namespace detail;

namespace p2p::detail
{

    struct wpa_ctrl
    {
#ifdef CONFIG_CTRL_IFACE_UDP
#ifdef CONFIG_CTRL_IFACE_UDP_IPV6
        struct sockaddr_in6 local;
        struct sockaddr_in6 dest;
#else  /* CONFIG_CTRL_IFACE_UDP_IPV6 */
        struct sockaddr_in local;
        struct sockaddr_in dest;
#endif /* CONFIG_CTRL_IFACE_UDP_IPV6 */
        char *cookie;
        char *remote_ifname;
        char *remote_ip;
#endif /* CONFIG_CTRL_IFACE_UDP */
#ifdef CONFIG_CTRL_IFACE_UNIX
        int s;
        struct sockaddr_un local;
        struct sockaddr_un dest;
#endif /* CONFIG_CTRL_IFACE_UNIX */
#ifdef CONFIG_CTRL_IFACE_NAMED_PIPE
        HANDLE pipe;
#endif /* CONFIG_CTRL_IFACE_NAMED_PIPE */
    };
}

WpaCtrl::WpaCtrl()
{
    ctrl = new wpa_ctrl();
    memset(ctrl,0,sizeof(*ctrl));
}
WpaCtrl::WpaCtrl(const std::string &deviceName)
{
    ctrl = new wpa_ctrl();
    memset(ctrl,0,sizeof(*ctrl));
    try {
        Open(deviceName);
    } catch (const std::exception &e)
    {
        delete ctrl;
        ctrl = nullptr;
        throw;
    }
}


WpaCtrl::~WpaCtrl()
{
    if (ctrl != nullptr)
    {
        if (ctrl->s != 1)
        {
            close(ctrl->s);
        }
        delete ctrl;
        ctrl = nullptr;
    }
}

static std::atomic<uint64_t> instanceId;

void WpaCtrl::Open(const std::string &socketName)
{
    Open(socketName, nullptr);
}
void WpaCtrl::Open(const std::string& socketName, const char*tempFilePath)
{
    if (tempFilePath != nullptr && tempFilePath[0] != '/')
    {
        throw invalid_argument("tempFileDirectory must start with '/'");
    }
    filesystem::path localSocketDirectory (
        tempFilePath 
        ? tempFilePath
        : CONFIG_CTRL_IFACE_CLIENT_DIR);


    //struct wpa_ctrl *ctrl;
    int tries = 0;
    int flags;


     int s = socket(PF_UNIX, SOCK_DGRAM, 0);
    const std::string tempFileName = SS("hp2p-" << getpid() << "-" << (instanceId++));
    auto clientPath = (localSocketDirectory / tempFileName).string();

    auto socketCleanup = finally([&] () {

        if (s != -1)
        {
            close(s);
            s = -1;
        }
        filesystem::remove(clientPath);
    });

    ctrl->local.sun_family = AF_UNIX;


    if (clientPath.length() >= sizeof(ctrl->local.sun_path)-1 )
    {
        throw invalid_argument("Socket name too long: " + clientPath);
    }
    memcpy(ctrl->local.sun_path,clientPath.c_str(),clientPath.length());

try_again:

#ifdef ANDROID
    /* Set client socket file permissions so that bind() creates the client
	 * socket with these permissions and there is no need to try to change
	 * them with chmod() after bind() which would have potential issues with
	 * race conditions. These permissions are needed to make sure the server
	 * side (wpa_supplicant or hostapd) can reply to the control interface
	 * messages.
	 *
	 * The lchown() calls below after bind() are also part of the needed
	 * operations to allow the response to go through. Those are using the
	 * no-deference-symlinks version to avoid races. */
    fchmod(s, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#endif /* ANDROID */
    if (bind(ctrl->s, (struct sockaddr *)&ctrl->local,
             sizeof(ctrl->local)) < 0)
    {
        if (errno == EADDRINUSE && tries < 2)
        {
            /*
             * replicates logic in wpa_ctrl.c. Seems improbable that we will recover, 
             * frankly. The argument: a previous process had our process id, and terminated
             * abnormally, leaving the file in place. It must have been us, argues, wpa_ctrl.c,
             * So remove it it and try again.
			 */
            std::filesystem::remove(ctrl->local.sun_path);
            goto try_again;
        }
        WpaIoException::ThrowErrno(); // throw an i/o exception.
    }

#ifdef ANDROID
    // this seems like an improbabe scenario; but preserve the code anyway.
    static_assert(false, "This codepath untested. Hard to say if it will still work.");

    /* Set group even if we do not have privileges to change owner */
    lchown(ctrl->local.sun_path, -1, AID_WIFI);
    lchown(ctrl->local.sun_path, AID_SYSTEM, AID_WIFI);

    if (os_strncmp(ctrl_path, "@android:", 9) == 0)
    {
        if (socket_local_client_connect(
                s, ctrl_path + 9,
                ANDROID_SOCKET_NAMESPACE_RESERVED,
                SOCK_DGRAM) < 0)
        {
            WpaIoException::ThrowErrno();
        }
        // will yu be able to select() the socket?
        static_assert(false);
        this->file.Attach(s); // pepare for select.
        socketCleanup.disable();
        return;

    }

    /*
	 * If the ctrl_path isn't an absolute pathname, assume that
	 * it's the name of a socket in the Android reserved namespace.
	 * Otherwise, it's a normal UNIX domain socket appearing in the
	 * filesystem.
	 */
    if (*ctrl_path != '/')
    {
        char buf[21];
        os_snprintf(buf, sizeof(buf), "wpa_%s", ctrl_path);
        if (socket_local_client_connect(
                ctrl->s, buf,
                ANDROID_SOCKET_NAMESPACE_RESERVED,
                SOCK_DGRAM) < 0)
        {
            WpaIoSocket::ThrowErrno();
        }
        coFile.Attach(s); // pepare for select.
        socketCleanup.disable();

        return;
    }
#endif /* ANDROID */

    if (socketName.length() >= sizeof(ctrl->dest.sun_family)-1)
    {
        throw invalid_argument("Socket name too long: " + socketName);
    }
    ctrl->dest.sun_family = AF_UNIX;

    memcpy(ctrl->dest.sun_path,socketName.c_str(),socketName.length());

    if (connect(s, (struct sockaddr *)&ctrl->dest,
                sizeof(ctrl->dest)) < 0)
    {
        WpaIoException::ThrowErrno();
    }

    /*
	 * Make socket non-blocking so that we don't hang forever if
	 * target dies unexpectedly.
	 */
    flags = fcntl(s, F_GETFL);
    if (flags >= 0)
    {
        flags |= O_NONBLOCK;
        if (fcntl(s, F_SETFL, flags) < 0)
        {
            /* definitely fatal. */
            WpaIoException::ThrowErrno();
        }
    }
    socketCleanup.disable();
    coFile.Attach(s);
    return;
}
void WpaCtrl::Close()
{
    coFile.Close();
}


CoTask<int>  WpaCtrl::CtrlRequest(const char *cmd, size_t cmd_len,
		     char *reply, size_t *reply_len)
{
    
    CoLockGuard lock;
    co_await lock.CoLock(requestMutex);

    co_await coFile.CoSend(cmd,cmd_len);


    while (true) {
        size_t received = co_await coFile.CoRecv(reply,*reply_len);
        reply[received] = '\0'; // always zero-terminate.
        if ((received > 0 && reply[0] == '<')  
        || (received > 6 && strncmp(reply,"IFNAME=",7) == 0))
        {
            // if (msg_cb)
            // {
            //     // Not really implemented. You need to use CoTask scheduling to service sockets.
            //     msg_cb(reply,received);
            // } else
            {
                // Expected scenarios: 
                // 1) Requests on one instance of WpaCtrl; events on another.
                // 2) Requests only, no events.
                // Explicitly not supported: trying to do both on the same channel. 
                throw logic_error("Received event message on a request socket. (Use one instance of WpaCtrl to request, and a second to service events)");
            }
        } else{
            co_return received;
        }
    }
}
