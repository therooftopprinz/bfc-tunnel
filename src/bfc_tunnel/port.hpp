#ifndef BFC_TUNNEL_PORT_HPP
#define BFC_TUNNEL_PORT_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>

namespace bfc_tunnel
{

using port_id_t = uint16_t;

class port : public std::enable_shared_from_this<port>
{
public:
    port() = delete;
    port(
        cv_reactor_ptr_t cv_reactor,
        port_id_t port_id);
    ~port();

    void initialize(transport_queue_pair_ptr_t transport);
    void uninitialize();

private:
    void on_transport_out_ready();

    port_id_t port_id;
    cv_reactor_ptr_t cv_reactor;
    transport_queue_pair_ptr_t transport;
};

using port_ptr_t  = std::shared_ptr<port>;
using port_wptr_t = std::weak_ptr<port>;
using port_map_t  = std::unordered_map<port_id_t, port_ptr_t>;
using port_rmap_t = std::unordered_map<transport_queue_pair*, port_wptr_t>;

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_PORT_HPP
