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
     * @see toInt<T>(const std::string & ), toUint<T>(const std::string & ), toUint64(const std::string & ), to toUint64(const std::string & )
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
     * @see toInt<T>(const std::string & ), toInt64(const std::string & ), to toUint64(const std::string & )
     */
    template <std::integral T>
    T toInt(const std::string&value)
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
     * @brief Specialization of toInt<T> for int64_t.
     * 
     * @see toInt<T>(const std::string & ), toInt64(const std::string & ), to toUint64(const std::string & )
     */
    template <>
    inline int64_t toInt<int64_t>(const std::string&value)
    {
        return toInt64(value);
    }
    /**
     * @brief Specialization of toInt<T> for uint64_t.
     * 
     * @see toInt<T>(const std::string & ), toUint64(const std::string & ), to toUint64(const std::string & )
     */
    template <>
    inline uint64_t toInt<uint64_t>(const std::string&value)
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

} // namepace.
