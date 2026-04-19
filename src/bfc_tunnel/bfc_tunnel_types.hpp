#ifndef BFC_TUNNEL_TYPES_HPP
#define BFC_TUNNEL_TYPES_HPP

#include "bfc/function.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>

#include <bfc/buffer.hpp>
#include <bfc/cv_reactor.hpp>
#include <bfc/default_reactor.hpp>
#include <bfc/socket.hpp>

namespace bfc_tunnel
{

using reactor_cb_t = typename bfc::function_type_helper<64, void()>::type;
using cv_reactor_t = bfc::cv_reactor<reactor_cb_t>;
using io_reactor_t = bfc::default_reactor<reactor_cb_t>;
using io_reactor_ptr_t = std::shared_ptr<io_reactor_t>;
using cv_reactor_ptr_t = std::shared_ptr<cv_reactor_t>;

struct node_id_s
{
    uint8_t domain;
    uint64_t csprng;
    uint64_t ts;
    
    bool operator==(const node_id_s& other) const
    {
        return domain == other.domain && csprng == other.csprng && ts == other.ts;
    }
    bool operator!=(const node_id_s& other) const
    {
        return !(*this == other);
    }
};

struct node_io_pdu_s
{
    sockaddr_storage addr;
    socklen_t addr_len;
    bfc::buffer pdu;
};

struct node_io_sdu_s
{
    node_id_s node_id;
    bfc::buffer sdu;
};

using node_transport_queue_t = bfc::reactive_event_queue<node_io_pdu_s, reactor_cb_t>;
using transport_node_queue_t = bfc::reactive_event_queue<node_io_pdu_s, reactor_cb_t>;
using node_service_queue_t   = bfc::reactive_event_queue<node_io_sdu_s, reactor_cb_t>;
using service_node_queue_t   = bfc::reactive_event_queue<node_io_sdu_s, reactor_cb_t>;

using node_transport_queue_ptr_t = std::shared_ptr<node_transport_queue_t>;
using transport_node_queue_ptr_t = std::shared_ptr<transport_node_queue_t>;
using node_service_queue_ptr_t   = std::shared_ptr<node_service_queue_t>;
using service_node_queue_ptr_t   = std::shared_ptr<service_node_queue_t>;

} // namespace bfc_tunnel

namespace std
{
template <>
struct hash<bfc_tunnel::node_id_s>
{
    std::size_t operator()(const bfc_tunnel::node_id_s& id) const noexcept
    {
        std::size_t h = id.domain;
        h ^= static_cast<std::size_t>(id.csprng) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= static_cast<std::size_t>(id.ts) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};
} // namespace std

#endif // BFC_TUNNEL_TYPES_HPP
