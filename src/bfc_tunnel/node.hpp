#ifndef BFC_TUNNEL_NODE_HPP
#define BFC_TUNNEL_NODE_HPP

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <bfc/sized_buffer.hpp>
#include <bfc/cv_reactor.hpp>

#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/security/dh.hpp>
#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/btprotocol.hpp>
#include <bfc_tunnel/security/dh.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>
#include <bfc_tunnel/security/dh.hpp>

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

using ports_t             = std::vector<port_ptr_t>;

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
    key_t      base_key_dh_eb;
    key_t      base_key_dh_ab;
    key_t      base_key_dh_ef;
    key_t      base_key_dh_af;
    std::vector<std::pair<uint8_t, key_t>> int_algo_keys;
    std::vector<std::pair<uint8_t, key_t>> conf_algo_keys;
    timer_id_t timer_id;
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

using peer_public_keys_t = std::vector<peer_public_key_s>;
using public_keys_t      = std::unordered_map<node_id_t, peer_public_keys_t>;

struct peer_transaction_s
{
    peer_address_s    address;
    uint8_t           id;
    bfc::sized_buffer data;
    completion_cb_t   callback;
};

using peer_transaction_t = std::optional<peer_transaction_s>;
using peer_transactions_t = std::deque<peer_transaction_s>;

struct peer_link_ctx_s
{
    uint64_t last_activity_time_us = 0;
    uint64_t sent_pkt = 0;
    uint64_t recv_pkt = 0;
    uint64_t sent_byt = 0;
    uint64_t recv_byt = 0;
};

using peer_links_t = std::vector<std::pair<peer_address_s, peer_link_ctx_s>>;

struct sec_proc_ctx_s
{
    dhke_kk dhke;
};

using sec_proc_ctx_t = std::optional<sec_proc_ctx_s>;

struct peer_s
{
    static constexpr uint64_t k_peer_static  = 1 >> 0;
    uint64_t                  flags = 0;
    node_id_t                 node_id = 0;
    peer_public_keys_t        public_keys;
    sec_ctx_map               sec_ctxs = {};
    peer_address_s            preffered_peer_address = {};
    peer_links_t              links;
    peer_transaction_t        current_transaction;
    peer_transactions_t       pending_transactions;
    uint32_t                  next_ctr = 0;
    timer_id_t                transaction_timer_id;
    sec_proc_ctx_t            sec_proc_ctx;
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
    timer_id_t     beacon_timer_id = {};
};

using beacon_ptr_t = std::shared_ptr<beacon_s>;
using beacons_t = std::vector<beacon_ptr_t>;

struct downstream_identity_s
{
    node_id_t node_id;
    key_t     private_key;

    sockaddr_t downstream_address;
    transport_queue_pair_ptr_t transport;
};

using downstream_identity_ptr_t = std::shared_ptr<downstream_identity_s>;
using downstream_identities_t = std::vector<downstream_identity_ptr_t>;

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

    void add_port                    (const port_ptr_t& transport);
    void rem_port                    (const port_ptr_t& port);
    void add_static_peer             (const sockaddr_t& address);
    void rem_static_peer             (const sockaddr_t& address);
    void add_downstream_identity     (const downstream_identity_ptr_t& identity);
    void rem_downstream_identity     (const downstream_identity_ptr_t& identity);

private:
    void add_beacon(beacon_ptr_t beacon);
    void rem_beacon(const port_ptr_t&);
    void rem_beacon(const sockaddr_t&);

    void on_beacon_timer_expired                (const beacon_ptr_t& beacon);
    void send_beacon                            (const beacon_ptr_t& beacon);

    void on_port_in_queue_ready                 (const port_ptr_t& port);
    void handle_pdu                             (const port_ptr_t& port, const sockaddr_t& from, const sock_buff_t& pdu);
    void handle_beacon                          (const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame);

    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::msg1& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::msg2& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::exchange_network_keys& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::link_info& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::link_report& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::route_announce& msg);
    void handle_btp_message(const port_ptr_t& port,const frame_const_t& frame, cum::n2n_indication& msg);

    void peer_update_link_activity(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& from, size_t size);
    void peer_start_security_procedure(const peer_ptr_t& peer, bool force = false);

    cv_reactor_ptr_t cv_reactor;

    bool                        is_initialized = false;
    downstream_identity_ptr_t   selected_downstream_identity;    
    peer_by_node_id_map         peers;
    ports_t                     ports;
    beacons_t                   beacons;
    sockaddrs_t                 static_peers;
    downstream_identities_t     downstream_identities;
    public_keys_t               public_keys;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_HPP
