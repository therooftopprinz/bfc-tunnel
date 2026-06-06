#ifndef BFC_TUNNEL_STRING_UTILS_HPP
#define BFC_TUNNEL_STRING_UTILS_HPP

#include <charconv>
#include <optional>
#include <string>
#include <type_traits>

namespace bfc_tunnel
{
namespace utils
{

template <typename T>
std::optional<T> str_as(const std::string& str)
{
    static_assert(std::is_integral_v<T>, "str_as is only supported for integral types");

    T value{};
    const char* begin = str.data();
    const char* end = begin + str.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (std::errc{} != ec || ptr != end)
    {
        return std::nullopt;
    }

    return value;
}

} // namespace utils
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_STRING_UTILS_HPP
