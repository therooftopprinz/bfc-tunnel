#include <bfc_tunnel/transport/transport_types.hpp>

#include <cstring>

namespace bfc_tunnel
{

bool is_equal(const struct sockaddr *sa, const struct sockaddr *sb)
{
    if (sa->sa_family != sb->sa_family)
        return false;
    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in *a = (const struct sockaddr_in *)sa;
        const struct sockaddr_in *b = (const struct sockaddr_in *)sb;
        return (a->sin_port == b->sin_port &&
                a->sin_addr.s_addr == b->sin_addr.s_addr);
    }
    else if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *a = (const struct sockaddr_in6 *)sa;
        const struct sockaddr_in6 *b = (const struct sockaddr_in6 *)sb;
        return (a->sin6_port == b->sin6_port &&
                memcmp(a->sin6_addr.s6_addr, b->sin6_addr.s6_addr, sizeof(a->sin6_addr.s6_addr)) == 0 &&
                a->sin6_scope_id == b->sin6_scope_id);
    }
    return false;
}

bool is_equal(const sockaddr_t& a, const sockaddr_t& b)
{
    if (std::holds_alternative<sockaddr_none>(a) && std::holds_alternative<sockaddr_none>(b))
    {
        return true;
    }
    else if (std::holds_alternative<sockaddr_in>(a) && std::holds_alternative<sockaddr_in>(b))
    {
        return is_equal((const sockaddr*)&std::get<sockaddr_in>(a), (const struct sockaddr*)&std::get<sockaddr_in>(b));
    }
    else if (std::holds_alternative<sockaddr_in6>(a) && std::holds_alternative<sockaddr_in6>(b))
    {
        return is_equal((const sockaddr*)&std::get<sockaddr_in6>(a), (const struct sockaddr*)&std::get<sockaddr_in6>(b));
    }

    return false;
}

} // namespace bfc_tunnel
