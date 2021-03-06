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
#include <cstdint>
#include <cstddef>
#include <string>
#include <stdexcept>
#include <concepts>
#include <vector>

namespace p2p {
    /**
     * @brief Convert a string to a uint64_t.
     * 
     * @param value The string to parse.
     * 
     * Accepts either decimal or (if the value is prefixed with "0x") hexadecimal input.
     * 
     * @return The parsed value.
     * @throws std::invalid_argument if the supplied string is not a valid uint64_t.
     */
    uint64_t toUint64(const std::string & value);
    /**
     * @brief Convert a string to int64_t.
     * 
     * Accepts either decimal or (if the value is prefixed with "0x") hexadecimal input.
     * @param value The string to parse.
     * @return int64_t The parsed value.
     * @throws std::invalid_argument if the supplied string is not a valid int64_t.
     * @see ToInt<T>(const std::string & ), toUint<T>(const std::string & ), toUint64(const std::string & ), to toUint64(const std::string & )
     * 
     */
    int64_t toInt64(const std::string & value);

    /**
     * @brief Convert a string to an integer.
     * 
     * @tparam T A signed or unsigned integral type.
     * @param value The string to parse.
     * @return T The parsed result.
     * @throws std::invalid_argument if the supplied string is not a valid integer.
     * @throws std::range_error if the resulting value does not fit into T.
     * @see ToInt<T>(const std::string & ), toInt64(const std::string & ), to toUint64(const std::string & )
     */
    template <std::integral T>
    T ToInt(const std::string&value)
    {
        int64_t result = toInt64(value);
        T typedResult = (T)result;
        if (typedResult != result)
        {
            throw std::range_error("Out of range.");
        }
        return typedResult;
    }

    /**
     * @brief Specialization of ToInt<T> for int64_t.
     * 
     * @see ToInt<T>(const std::string & ), toInt64(const std::string & ), to toUint64(const std::string & )
     */
    template <>
    inline int64_t ToInt<int64_t>(const std::string&value)
    {
        return toInt64(value);
    }
    /**
     * @brief Specialization of ToInt<T> for uint64_t.
     * 
     * @see ToInt<T>(const std::string & ), toUint64(const std::string & ), to toUint64(const std::string & )
     */
    template <>
    inline uint64_t ToInt<uint64_t>(const std::string&value)
    {
        return toUint64(value);
    }

    /**
     * @brief Split the supplied string using the supplied delimiter.
     * 
     * @param value Input value.
     * @param delimiter The character to use as a list delimiter.
     * @return std::vector<std::string> 
     */
    std::vector<std::string> split(const std::string &value, char delimiter);

    /**
     * @brief Split a list of wpa_supplicant flags.
     * 
     * Parses an input of type wpa_supplicant-style flags into a list of separated flags. 
     * 
     * e.g.  `splitWpaFlags("[flag1][flag2][flagn...]")` returns `std::vector<std::string> { "[flag1]", "[flag2}", "[flagn...]"}`.
     * 
     * @param value The input value to split.
     * @return std::vector<std::string> 
     */
    std::vector<std::string> splitWpaFlags(const std::string &value);

    /**
     * @brief return a string containing random alpha-numeric characters.
     * 
     * @param length 
     * @return std::string 
     */
    std::string randomText(size_t length);
    
    namespace detail {
        template <typename F>
        struct FinalAction {
            FinalAction(F f) : clean_{f} {}
        ~FinalAction() { if(enabled_) clean_(); }
            void disable() { enabled_ = false; };
        private:
            F clean_;
            bool enabled_{true}; 
        };
    } // namespace

    template <typename F>
    detail::FinalAction<F> finally(F f) {
        return detail::FinalAction<F>(f); 
    }

    std::string toHex(uint8_t byteVal);
    std::string toHex(uint16_t byteVal);
    std::string toHex(size_t length, const void*data);
    std::string toHex(const std::vector<uint8_t> &data);

    bool isValidDnsSdName(const std::string &value);
    
    char ansiToLower(char c);
    char ansiToUpper(char c);
    std::string ansiToLower(const std::string &value);
    std::string ansiToUpper(const std::string &value);
    bool caseInsensitiveCompare(const std::string &left, const std::string&right);
} // namepace.
