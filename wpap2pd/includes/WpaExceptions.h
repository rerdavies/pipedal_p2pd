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