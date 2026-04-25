#ifndef BFC_TUNNEL_TRANSPORT_TYPES_HPP
#define BFC_TUNNEL_TRANSPORT_TYPES_HPP

#include <bfc/buffer.hpp>
#include <bfc/socket.hpp>
#include <bfc/cv_reactor.hpp>

#include <variant>

#include <bfc_tunnel/bfc_tunnel_types.hpp>

namespace bfc_tunnel
{

struct generic_transport_address_s
{};

struct udp_multicast_address_s
{
    std::string address
};

struct udp_unicast_address_s
{
    std::string group_address;
    std::string interface;
};

using ip_address_s = std::variant<sockaddr_in, sockaddr_in6>;

struct transport_data_s
{
    std::optional<ip_address_s> address;
    bfc::buffer data;
};

enum transport_type_e
{
    E_TRANSPORT_TYPE_GENERIC_OVER_UDP4,
    E_TRANSPORT_TYPE_UDP4_MULTICAST,
    E_TRANSPORT_TYPE_UDP4_UNICAST,
};

inline constexpr uint64_t E_TRANSPORT_CAPABILITY_HUB = 1 << 0;

struct transport_config_s
{
    transport_type_e type;
    uint64_t capabilities;
    transport_address_s bind_address;
    std::vector<transport_address_s> peers;
};

using transport_in_t  = std::variant<transport_data_s>;
using transport_out_t = std::variant<transport_config_s, transport_data_s>;

using transport_in_queue_t  = bfc::reactive_event_queue<transport_in_t, reactor_cb_t>;
using transport_out_queue_t = bfc::reactive_event_queue<transport_out_t, reactor_cb_t>;
    
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_TYPES_HPP
