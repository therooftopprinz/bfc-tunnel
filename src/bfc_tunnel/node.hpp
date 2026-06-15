#ifndef BFC_TUNNEL_NODE_HPP
#define BFC_TUNNEL_NODE_HPP

#include <memory>
#include <vector>

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/btprotocol.hpp>
#include <bfc_tunnel/port.hpp>

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
    port_map_t ports;
    port_rmap_t port_refs;
    uint16_t port_id_counter = 0;

    enum node_state_e
    {
        E_NODE_STATE_UNINITIALIZED,
        E_NODE_STATE_INITIALIZED
    };
    node_state_e state = E_NODE_STATE_UNINITIALIZED;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_HPP
