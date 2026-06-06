#ifndef BFC_TUNNEL_NODE_HPP
#define BFC_TUNNEL_NODE_HPP

#include <memory>
#include <vector>

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/btprotocol.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>

namespace bfc_tunnel
{

class node : public std::enable_shared_from_this<node>
{
public:
    explicit node(cv_reactor_ptr_t cv_reactor);
    ~node();

    void initialize();
    void uninitialize();

    void add_transport(transport_queue_pair_ptr_t transport);

private:
    void on_transport_out_ready(transport_queue_pair_ptr_t transport);

    void handle_transport_out(const transport_identity_s& data);
    void handle_transport_out(const transport_data_s& data);
    void handle_transport_out(const transport4_data_s& data);
    void handle_transport_out(const transport6_data_s& data);
    void handle_transport_out(const transport_delivery_failure& data);

    void handle_pdu(const bfc::const_buffer_view& pdu);

    void handle_btp_message(const cum::BTPMessage& msg);
    void handle_btp_message(const cum::beacon& msg);
    void handle_btp_message(const cum::msg1& msg);
    void handle_btp_message(const cum::msg2& msg);
    void handle_btp_message(const cum::link_info& msg);
    void handle_btp_message(const cum::link_report& msg);
    void handle_btp_message(const cum::route_announce& msg);
    void handle_btp_message(const cum::n2n_indication& msg);

    cv_reactor_ptr_t cv_reactor;
    std::vector<transport_queue_pair_ptr_t> transports;

    enum node_state_e
    {
        E_NODE_STATE_UNINITIALIZED,
        E_NODE_STATE_INITIALIZED
    };
    node_state_e state = E_NODE_STATE_UNINITIALIZED;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_HPP
