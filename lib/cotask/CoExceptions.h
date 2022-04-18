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

    /**
     * @brief Attempt to read from or write to a closed file.
     * 
     */
    class CoIoClosedException : public CoIoException
    {
    public:
        using base = CoIoException;

        CoIoClosedException()
        :base(ENOTCONN, "Closed.")
        {
        }
    };


} // namespace.