#ifndef BFC_TUNNEL_UTILS_TIME_UTILS_HPP
#define BFC_TUNNEL_UTILS_TIME_UTILS_HPP

#include <chrono>
#include <cstdint>

namespace bfc_tunnel
{

inline uint64_t now_s()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_UTILS_TIME_UTILS_HPP
