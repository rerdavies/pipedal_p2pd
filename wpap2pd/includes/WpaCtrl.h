#pragma once

#include <string>
#include "cotask/CoFile.h"
#include <filesystem>
#include "cotask/CoEvent.h"

namespace p2p {
    using namespace cotask;
    using namespace std;

    namespace detail {
        struct wpa_ctrl; // forward declaration.
    }
    /**
     * @brief Wrapper class for a platform-specific wpa_supplicant socket.
     *
     * Base on code contained in wpa_supplicant wpa_ctrl.c/wpa_ctrl.h. Ripped and stripped in order to 
     * avoid including the entire source tree for wpa_supplicant (deep undocument hander and .o dependencies for wpa_ctrl.h/wpa_ctrl.c).
     * 
     * Reference wpa_ctrl.c in order to determine how to implement ports of this file for platforms other than LINUX. (typically involves 
     * opening the correct type of pipe, depending on the OS.)
     * 
     */
    class WpaCtrl {
            // no copy, no move.
            WpaCtrl(const WpaCtrl&) = delete;
            WpaCtrl(WpaCtrl&&) = delete;
        public:
            WpaCtrl();
            WpaCtrl(const std::string &deviceName);
            ~WpaCtrl();


            /**
             * @brief Open a connection to wpa_supplicant.
             * 
             * @param socketName Name of the wpa_supplicant interface.
             */
            void Open(const std::string &interfaceName);
            /**
             * @brief Andriod-specific variant of Open().
             * 
             * @param socketName Full path of the wpa_supplication socket.
             * @param tempFilePath The directory in which to place local socket names.
             * 
             * Android temp-file directory is application specific. Allow Android
             * apps to indicate where they want their local sockets placed.
             */
            void Open(const std::string& socketName, const char*tempFilePath);

            /**
             * @brief Close the socket.
             * 
             * Close the underlying socket, cause all waiting coroutines to fail with 
             * an exception, and cause all subsequent coroutine i/o operations (exept Close)
             * to throw i/o exceptions.
             * 
             * See WpaCtrl for an important discussion of co-routine thread safety and lifetime 
             * management.
             */
            void Close();


            /**
             * @brief Receive events on a coroutine thread.
             * 
             * @param buffer The buffer into which data is stored.
             * @param size 
            *  @param timeout in milliseconds (optionaL) 

             * @return CoTask<size_t> 
             * @throws WapIoException on errors.
             * @throws WapClosedException on close.
             */
            CoTask<size_t> CoRecv(void *buffer, size_t size, std::chrono::milliseconds timeout = NO_TIMEOUT);



            CoTask<int> CtrlRequest(
                const char *cmd, size_t cmd_len,
		        char *reply, size_t *reply_len);

        private:
            CoMutex requestMutex;

            void Open2(const std::string &fullPath);
            class detail::wpa_ctrl *ctrl = nullptr; // os-dependent data
            CoFile coFile; // receiver for select notifications.

    };


    inline CoTask<size_t> WpaCtrl::CoRecv(void *buffer, size_t size, std::chrono::milliseconds timeout)
    {
        return coFile.CoRecv(buffer,size,timeout);
    }


}