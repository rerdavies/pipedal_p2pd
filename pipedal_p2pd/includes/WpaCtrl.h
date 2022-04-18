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

            CoTask<> Attach();
            CoTask<> Detach();

            CoTask<size_t> CoRequest(
                const char *cmd, size_t cmd_len,
		        char *reply, size_t buffer_length);

        private:
            CoTask<> AttachHelper(const std::string &message);

            void Open2(const std::string &fullPath);
            class detail::wpa_ctrl *ctrl = nullptr; // os-dependent data
            CoFile coFile; // receiver for select notifications.
            std::string socketName;
    };


    inline CoTask<size_t> WpaCtrl::CoRecv(void *buffer, size_t size, std::chrono::milliseconds timeout)
    {
        return coFile.CoRecv(buffer,size,timeout);
    }


}