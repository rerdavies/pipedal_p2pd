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

#include "cotask/CoExceptions.h"

namespace p2p {
    using namespace cotask;

    /**
     * @brief Exception indicating in invalid wap_supplication action.
     * 
     */
    class WpaIoException : public CoIoException {
    public:
        using base = CoIoException;
        
        WpaIoException(int errNo,const std::string what)
        : base(errNo,what) {}
    };
    class WpaFailedException : public WpaIoException {
    public:
        using base = WpaIoException;
        WpaFailedException(const std::string &responseCode, const std::string &command)
        : base(EBADMSG,"Request failed. (" + responseCode + ") " + command)
        {}
    };
    
    /**
     * @brief Operation has been aborted due to disconnection.
     * 
     */
    class WpaDisconnectedException : public CoException {
    public:
        using base = CoException;
        
        WpaDisconnectedException() { }
        virtual const char*what() const noexcept {
            return "Disconnected.";
        }
    };
    /**
     * @brief Operation has timed out.
     * 
     */
    class WpaTimedOutException : public CoTimedOutException {
    public:
    };


} // namespace.