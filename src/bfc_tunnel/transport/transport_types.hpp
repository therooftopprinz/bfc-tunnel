#ifndef BFC_TUNNEL_TRANSPORT_TYPES_HPP
#define BFC_TUNNEL_TRANSPORT_TYPES_HPP

#include <bfc/sized_buffer.hpp>
#include <bfc/socket.hpp>
#include <bfc/cv_reactor.hpp>

#include <variant>
#include <memory>

#include <bfc_tunnel/bfc_tunnel_types.hpp>

namespace bfc_tunnel
{

struct sockaddr_none
{};

using sockaddr_t        = std::variant<sockaddr_none, sockaddr_in, sockaddr_in6>;
using sock_buff_t       = bfc::sized_buffer;

using sockaddrs_t = std::vector<sockaddr_t>;

bool is_equal(const struct sockaddr *sa, const struct sockaddr *sb);
bool is_equal(const sockaddr_t& a, const sockaddr_t& b);

struct transport_data_s
{
    uint64_t    id;
    sockaddr_t  address;
    sock_buff_t data;
};

struct transport_delivery_failure
{
    uint64_t id;
    sockaddr_t address;
    int error;
};

using transport_in_t  = transport_data_s;
using transport_out_t = std::variant<transport_data_s, transport_delivery_failure>;

using transport_in_queue_t  = bfc::reactive_event_queue<transport_in_t,  reactor_cb_t>;
using transport_out_queue_t = bfc::reactive_event_queue<transport_out_t, reactor_cb_t>;

struct transport_queue_pair_s
{
    transport_in_queue_t in;
    transport_out_queue_t out;
};

using transport_queue_pair_ptr_t = std::shared_ptr<transport_queue_pair_s>;

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_TYPES_HPP
