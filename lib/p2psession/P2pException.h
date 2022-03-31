#pragma once

#include <exception>
#include <string>
#include <utility>

namespace p2psession {
    class P2pException : std::exception {

    public:
        P2pException(std::string&&what)
        :mWhat(what)
        {

        }
        P2pException(const std::string &what)
        :mWhat(what)
        {

        }
        const char*what() const noexcept {
            return mWhat.c_str();
        }
    private:
        std::string mWhat;

    };

    class P2pCancelledException : std::exception {

    public:
        P2pCancelledException()
        {

        }
        const char*what() const noexcept {
            return "Task cancelled.";
        }
    };
    class P2pTimeoutException : std::exception {

    public:
        P2pTimeoutException()
        {
        }
        const char*what() const noexcept {
            return "Timed out.";
        }

    };

}