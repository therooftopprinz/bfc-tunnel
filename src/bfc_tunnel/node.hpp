#ifndef BFC_TUNNEL_NODE_HPP
#define BFC_TUNNEL_NODE_HPP

#include <functional>
#include <memory>

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol.hpp>

namespace bfc_tunnel
{

class node : public std::enable_shared_from_this<node>
{
public:
    node(
        std::shared_ptr<cv_reactor> reactor,
        node_transport_queue_ptr transport_out,
        transport_node_queue_ptr transport_in,
        node_service_queue_ptr service_out,
        service_node_queue_ptr service_in
    );
    ~node();

    void initialize();
    void uninitialize();

private:
    void on_transport_in_queue_ready();
    void on_service_in_queue_ready();

    cv_reactor_ptr cv_reactor;
    node_transport_queue_ptr transport_out;
    transport_node_queue_ptr transport_in;
    node_service_queue_ptr service_out;
    service_node_queue_ptr service_in;

    enum node_state_e
    {
        E_NODE_STATE_UNINITIALIZED,
        E_NODE_STATE_INITIALIZED
    };
    node_state_e state = E_NODE_STATE_INITIALIZED;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_HPP