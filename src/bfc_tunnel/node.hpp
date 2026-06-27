#ifndef BFC_TUNNEL_NODE_HPP
#define BFC_TUNNEL_NODE_HPP

#include "bfc/sized_buffer.hpp"
#include "bfc_tunnel/protocol/frame.hpp"
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/btprotocol.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>
#include <bfc/cv_reactor.hpp>

namespace bfc_tunnel
{

/***********************************************
/
/            Forward Declarations
/
************************************************/
struct port_s;
struct security_context_s;
struct peer_s;
struct peer_link_ctx_s;

using port_ptr_t          = std::shared_ptr<port_s>;
using peer_ptr_t          = std::shared_ptr<peer_s>;
using peer_link_ctx_ptr_t = std::shared_ptr<peer_link_ctx_s>;

/***********************************************
/
/            Port Definitions
/
************************************************/
struct port_s
{
    enum port_type_e
    {
        UNICAST,
        MULTICAST
    };

    std::string                name;
    port_type_e                type;
    sockaddr_t                 interface_address;
    transport_queue_pair_ptr_t transport;
};

using port_by_address_map = std::unordered_map<std::string, port_ptr_t>;

/***********************************************
/
/            Security Context Definitions
/
************************************************/

struct security_context_s
{
    key_t    key;
    uint8_t  confidentiality_algorithm;
    uint8_t  integrity_algorithm;
    uint64_t expiration_time_us;
};

using sec_ctx_map = std::unordered_map<uint8_t, security_context_s>;

/***********************************************
/
/            Peer Definitions
/
************************************************/

struct peer_address_s
{
    port_ptr_t port;
    sockaddr_t address;
};


struct peer_public_key_s
{
    uint8_t    key_type;
    key_t      public_key;
};

struct peer_transaction_s
{
    peer_address_s    address;
    uint8_t           id;
    bfc::sized_buffer data;
    completion_cb_t   callback;
};

struct peer_link_ctx_s
{
    uint64_t last_activity_time_us = 0;
    uint64_t sent_pkt = 0;
    uint64_t recv_pkt = 0;
    uint64_t sent_byt = 0;
    uint64_t recv_byt = 0;
};

struct peer_s
{
    static constexpr uint64_t k_peer_static  = 1 >> 0;

    uint64_t       peer_flags = 0;
    node_id_t      node_id = 0;
    std::vector<peer_public_key_s>
                   public_keys;
    sec_ctx_map    sec_ctxs = {};
    peer_address_s preffered_port = {};
    std::vector<std::pair<peer_address_s, peer_link_ctx_s>>
                   ports;

    std::optional<peer_transaction_s> current_transaction;
    std::deque<peer_transaction_s>    pending_transactions;
    cv_reactor_t::timer_t::timer_id_t transaction_timer_id;
};

using peer_by_node_id_map = std::unordered_map<node_id_t, peer_ptr_t>;

/***********************************************
/
/            Beacon Definitions
/
************************************************/

struct beacon_s
{
    peer_address_s peer;
    uint64_t       beacon_interval_ms = 500;
    cv_reactor_t::timer_t::timer_id_t
                   beacon_timer_id = {};
};

using beacon_ptr_t = std::shared_ptr<beacon_s>;

/***********************************************
/
/            Node Definitions
/
************************************************/

class node : public std::enable_shared_from_this<node>
{
public:
    explicit node(cv_reactor_ptr_t cv_reactor);
    ~node();

    void initialize();
    void uninitialize();

    void add_port(port_ptr_t transport);
    void rem_port(port_ptr_t port);

    void add_static_peer(const sockaddr_t& address);
    void rem_static_peer(const sockaddr_t& address);

private:
    void add_beacon(beacon_ptr_t beacon);
    void rem_beacon(const port_ptr_t&);
    void rem_beacon(const sockaddr_t&);

    void on_beacon_timer_expired(beacon_ptr_t beacon);
    void send_beacon(beacon_ptr_t beacon);

    void on_port_in_queue_ready(port_ptr_t port);
    void handle_pdu(const port_ptr_t& port, const sock_buff_t& pdu);
    void handle_beacon(const port_ptr_t& port, const frame_const_t& frame);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::msg1& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::msg2& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::exchange_network_keys& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::link_info& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::link_report& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::route_announce& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::n2n_indication& msg);

    cv_reactor_ptr_t cv_reactor;

    enum node_state_e
    {
        E_NODE_STATE_UNINITIALIZED,
        E_NODE_STATE_INITIALIZED                                  
    };

    node_state_e state = E_NODE_STATE_UNINITIALIZED;

    node_id_t node_id = 0;

    peer_by_node_id_map       peers;
    std::vector<port_ptr_t>   ports;
    std::vector<beacon_ptr_t> beacons;
    std::vector<sockaddr_t>   static_peers;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_HPP
