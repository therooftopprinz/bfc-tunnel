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
        cv_reactor_ptr_t reactor,
        node_transport_queue_ptr_t transport_out,
        transport_node_queue_ptr_t transport_in,
        node_service_queue_ptr_t service_out,
        service_node_queue_ptr_t service_in
    );
    ~node();

    void initialize();
    void uninitialize();

private:
    void on_transport_in_queue_ready();
    void on_service_in_queue_ready();

    void handle_transport_message(const header_s& header, const bfc::buffer_view& payload);
    void handle_transport_message(const header_s& header, const id_request_s& payload);
    void handle_transport_message(const header_s& header, const id_response_s& payload);
    void handle_transport_message(const header_s& header, const link_info_s& payload);
    void handle_transport_message(const header_s& header, const link_report_s& payload);
    void handle_transport_message(const header_s& header, const route_announce_s& payload);
    void handle_transport_message(const header_s& header, const hub_announce_s& payload);
    void handle_transport_message(const header_s& header, const n2n_indication_s& payload);
    void handle_transport_message(const header_s& header, const tunnel_data_s& payload);

    cv_reactor_ptr_t cv_reactor;
    node_transport_queue_ptr_t transport_out;
    transport_node_queue_ptr_t transport_in;
    node_service_queue_ptr_t service_out;
    service_node_queue_ptr_t service_in;

    enum node_state_e
    {
        E_NODE_STATE_UNINITIALIZED,
        E_NODE_STATE_INITIALIZED
    };
    node_state_e state = E_NODE_STATE_INITIALIZED;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_HPP