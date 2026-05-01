#ifndef BFC_TUNNEL_TRANSPORT_TYPES_HPP
#define BFC_TUNNEL_TRANSPORT_TYPES_HPP

#include <bfc/buffer.hpp>
#include <bfc/socket.hpp>
#include <bfc/cv_reactor.hpp>

#include <variant>
#include <optional>

#include <bfc_tunnel/bfc_tunnel_types.hpp>

namespace bfc_tunnel
{

enum transport_type_e
{
    E_TRANSPORT_TYPE_GENERIC_OVER_UDP4,
    E_TRANSPORT_TYPE_UDP4_MULTICAST,
    E_TRANSPORT_TYPE_UDP6_MULTICAST,
    E_TRANSPORT_TYPE_UDP4_UNICAST,
    E_TRANSPORT_TYPE_UDP6_UNICAST,
};

struct transport_config_s
{
    transport_type_e type;
    std::string bind_address;
    uint16_t bind_port;
    std::vector<std::pair<std::string, uint16_t>> peers;
};

using ip_address_s = std::variant<sockaddr_in, sockaddr_in6>;

struct transport_data_s
{
    bfc::buffer data;
};

struct transport4_data_s
{
    sockaddr_in address;
    bfc::buffer data;
};

struct transport6_data_s
{
    sockaddr_in6 address;
    bfc::buffer data;
};

using transport_in_t  = std::variant<transport_data_s, transport4_data_s, transport6_data_s>;
using transport_out_t = std::variant<transport_config_s, transport4_data_s, transport6_data_s>;

using transport_in_queue_t  = bfc::reactive_event_queue<transport_in_t,  reactor_cb_t>;
using transport_out_queue_t = bfc::reactive_event_queue<transport_out_t, reactor_cb_t>;
    
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_TYPES_HPP
