#pragma once
#include <string>

namespace cotask {

    class CoException: public std::exception
    {
    public:
        using base = std::exception;
        virtual ~CoException() { }
        CoException() {}
        virtual const char *what() const noexcept = 0;

    };
    class CoCancelledException : public CoException
    {
    public:
        using base = CoException;
        CoCancelledException()
            {}

        virtual const char *what() const noexcept
        {
            return "Cancelled.";
        }
    };
    class CoTimedOutException : public CoException
    {
    public:
        using base = CoException;
        
        CoTimedOutException() {}

        virtual const char *what() const noexcept
        {
            return "Timed out.";
        }
    };

    /**
     * @brief An i/o exception.
     * 
     * ErrNo() contains an errno error code.
     */
    class CoIoException : public CoException
    {
    public:
        using base = CoException;
        /**
         * @brief Throw an CoIoException using the current value of errno.
         * 
         */
        [[noreturn]] static void ThrowErrno();

        CoIoException(const CoIoException & other)
        :what_(other.what_),errNo_(other.errNo_) {}
        
        virtual ~CoIoException() {

        }
        /**
         * @brief Construct a new Async Io Exception object
         * 
         * @param errNo an errno error code.
         * @param what text for the error.
         */
        CoIoException(int errNo, const std::string &what)
            : what_ (what), errNo_(errNo)
        {
        }

        const char*what() const noexcept {
            return what_.c_str();
        }

        int errNo() const { return errNo_; }

    private:
        std::string what_;
        int errNo_;
    };

    /**
     * @brief File not found exception.
     * 
     */
    class CoFileNotFoundException: public CoIoException 
    {
    public:
        using base = CoIoException;
        CoFileNotFoundException(const std::string & what)
        : base(EACCES ,what) { }
    };

    /**
     * @brief Attempt to read past end of file.
     * 
     */
    class CoEndOfFileException : public CoIoException
    {
    public:
        using base = CoIoException;
        /**
         * @brief Construct a new Async End Of File Exception object
         * 
         */
        CoEndOfFileException() : CoIoException(ENODATA,"End of file.") {}
    };


} // namespace.