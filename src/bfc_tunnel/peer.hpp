#ifndef BFC_TUNNEL_PEER_HPP
#define BFC_TUNNEL_PEER_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>
#include <bfc_tunnel/port.hpp>
#include <unordered_map>
#include <unordered_set>

namespace bfc_tunnel
{

struct peer_transport_s
{
    sockaddr_t  address;
    port_wptr_t port;
};

class peer
{
public:
    peer(node_id_t node_id);
    ~peer();

private:
    node_id_t node_id;
    peer_transport_s preffered_address;
    std::unordered_set<peer_transport_s> addresses;
};

using peer_ptr_t = std::shared_ptr<peer>;
using peer_map_t = std::unordered_map<node_id_t, peer_ptr_t>;

} // namespace bfc_tunnel
#endif // BFC_TUNNEL_PEER_HPP