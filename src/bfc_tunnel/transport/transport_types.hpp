#ifndef BFC_TUNNEL_TRANSPORT_TYPES_HPP
#define BFC_TUNNEL_TRANSPORT_TYPES_HPP

#include <bfc/buffer.hpp>
#include <bfc/socket.hpp>
#include <bfc/cv_reactor.hpp>

#include <variant>
#include <memory>

#include <bfc_tunnel/bfc_tunnel_types.hpp>

namespace bfc_tunnel
{

enum transport_type_e
{
    E_TRANSPORT_TYPE_GENERIC_OVER_UDP4,
    E_TRANSPORT_TYPE_UDP_MULTICAST,
    E_TRANSPORT_TYPE_UDP_UNICAST,
};

struct transport_query_identity_s
{};

struct transport_identity_s
{
    transport_type_e         type;
    std::string              address;
};

struct transport_data_s
{
    uint64_t id;
    bfc::buffer data;
};

struct transport4_data_s
{
    uint64_t id;
    sockaddr_in address;
    bfc::buffer data;
};

struct transport6_data_s
{
    uint64_t id;
    sockaddr_in6 address;
    bfc::buffer  data;
};

using sockaddr_t = std::variant<sockaddr_in, sockaddr_in6>;

struct transport_delivery_failure
{
    uint64_t id;
    sockaddr_t address;
    int error;
};

using transport_in_t  = std::variant<
    transport_query_identity_s,
    transport_data_s,
    transport4_data_s,
    transport6_data_s>;
using transport_out_t = std::variant<
    transport_identity_s,
    transport_data_s,
    transport4_data_s,
    transport6_data_s,
    transport_delivery_failure>;

using transport_in_queue_t  = bfc::reactive_event_queue<transport_in_t,  reactor_cb_t>;
using transport_out_queue_t = bfc::reactive_event_queue<transport_out_t, reactor_cb_t>;

struct transport_queue_pair_t
{
    transport_in_queue_t in;
    transport_out_queue_t out;
};

using transport_queue_pair_ptr_t = std::shared_ptr<transport_queue_pair_t>;

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_TYPES_HPP
