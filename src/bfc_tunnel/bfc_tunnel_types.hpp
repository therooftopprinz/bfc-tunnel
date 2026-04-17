#ifndef BFC_TUNNEL_TYPES_HPP
#define BFC_TUNNEL_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <functional>

#include <bfc/buffer.hpp>
#include <bfc/cv_reactor.hpp>
#include <bfc/default_reactor.hpp>

namespace bfc_tunnel
{

using cv_reactor = bfc::cv_reactor<std::function<void()>>;
using io_reactor = bfc::default_reactor<std::function<void()>>;

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

struct node_io_pduv4_s
{
    sockaddr_in addr;
    bfc::buffer pdu;
};

struct node_io_sdu_s
{
    node_id_s node_id;
    bfc::buffer sdu;
};

using node_transpport_in_queue_t  = bfc::reactive_event_queue<node_io_pduv4_s>;
using node_transpport_out_queue_t = bfc::reactive_event_queue<node_io_pduv4_s>;
using node_service_in_queue_t     = bfc::reactive_event_queue<node_io_sdu_s>;
using node_service_out_queue_t    = bfc::reactive_event_queue<node_io_sdu_s>;

struct node_transport_queues_s
{
    node_transpport_in_queue_t in;
    node_transpport_out_queue_t out;
};

using node_transport_queues_ptr = std::shared_ptr<node_transport_queues_s>;

struct node_service_queues_s
{
    node_service_in_queue_t in;
    node_service_out_queue_t out;
};

using node_service_queues_ptr = std::shared_ptr<node_service_queues_s>;

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
