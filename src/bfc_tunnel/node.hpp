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
#include <set>

namespace bfc_tunnel
{

/***********************************************
/
/            Forward Declarations
/
************************************************/
struct port_s;
struct security_ctx_s;
struct peer_s;
struct peer_link_ctx_s;

using port_ptr_t          = std::shared_ptr<port_s>;
using peer_ptr_t          = std::shared_ptr<peer_s>;
using peer_weak_ptr_t    = std::weak_ptr<peer_s>;
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

struct security_ctx_s
{
    uint8_t    id;
    uint64_t   creation_id;
    key_t      integrity_key;
    key_t      confidentiality_key;
    uint8_t    integrity_algorithm;
    uint8_t    confidentiality_algorithm;
    bool       is_expiring = false;
    timer_id_t timer_id;
};

using security_ctx_t  = std::optional<security_ctx_s>;
using security_ctxs_t = std::deque<security_ctx_s>;

/***********************************************
/
/            Peer Definitions
/
************************************************/

struct peer_address_s
{
    port_ptr_t        port;
    sockaddr_t        address;
};

struct peer_public_key_s
{
    dh_key_type_e     key_type;
    key_t             public_key;
};

using public_keys_t = std::unordered_map<node_id_t, peer_public_key_s>;

enum peer_transaction_status_e
{
    E_PEER_TRANSACTION_STATUS_OK,
    E_PEER_TRANSACTION_STATUS_MAX_RETRIES_REACHED,
    E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS,
    E_PEER_TRANSACTION_STATUS_VERIFICATION_FAILED,
    E_PEER_TRANSACTION_STATUS_UNSUPPORTED,
    E_PEER_TRANSACTION_STATUS_NO_SUPPORTED_ALGORITHM_PAIR,
    E_PEER_TRANSACTION_STATUS_PEER_TEARDOWN,
    E_PEER_TRANSACTION_STATUS_PEER_EXPIRED
};

const char* to_string(peer_transaction_status_e status);

struct peer_transaction_s
{
    uint8_t           id;
    transaction_cb_t  do_cb;
    completion_cb_t   done_cb;
    expiration_cb_t   expiration_cb;
};

using peer_transaction_t  = std::optional<peer_transaction_s>;
using peer_transactions_t = std::deque<peer_transaction_s>;

struct peer_link_ctx_s
{
    uint64_t last_activity_time_s = 0;
    uint64_t sent_pkt = 0;
    uint64_t recv_pkt = 0;
    uint64_t sent_byt = 0;
    uint64_t recv_byt = 0;
};

using peer_links_t = std::vector<std::pair<peer_address_s, peer_link_ctx_s>>;

using algo_pair_t = std::pair<uint8_t, uint8_t>;
using algorithm_pairs_t = std::set<algo_pair_t>;

struct security_procedure_ctx_s
{
    enum sec_proc_state_e
    {
        E_SEC_PROC_STATE_SENDER_QUEUED,
        E_SEC_PROC_STATE_SENDER_ONGOING,
        E_SEC_PROC_STATE_SENDER_EXPIRED
    };

    uint8_t             id;
    sec_proc_state_e    state = E_SEC_PROC_STATE_SENDER_ONGOING;
    unsigned            retry_count = 0;
    bfc::sized_buffer   pdu;
    frame_t             frame;
    cum::msg1           msg1;
    algo_pair_t         algo_pair_used;
    uint64_t            creation_id;

    dhke_kk             dhke;
};

using sec_proc_ctx_t = std::optional<security_procedure_ctx_s>;

struct peer_s
{
    static constexpr uint64_t k_peer_static       = 1 << 0;

    uint64_t                  flags = 0;
    node_id_t                 node_id = 0;
    peer_public_key_s         public_key;

    // security
    security_ctxs_t           security_contexts;
    uint8_t                   sec_ctx_ctr = 0;
    uint32_t                  tx_sn = 0;
    uint64_t                  context_creation_counter = 0;

    // transport
    peer_address_s            preferred_peer_address = {};
    peer_links_t              multicast_links;
    peer_links_t              unicast_links;
    uint64_t                  last_activity_time_s = 0;

    // transactions
    uint8_t                   trid_ctr = 0;
    peer_transaction_t        current_transaction;
    peer_transactions_t       pending_transactions;
    timer_id_t                transaction_timer_id;

    // procedures
    sec_proc_ctx_t            sec_proc_ctx;

    // config
    algorithm_pairs_t         test_algorithm_pairs;
    algorithm_pairs_t         supported_algorithm_pairs;
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

struct node_config_s
{
    uint64_t beacon_interval_ms             = 500;
    uint64_t check_peer_activity_interval_s = 15;
    uint64_t peer_timeout_s                 = 15;
    uint64_t peer_security_ctx_timeout_s    = 120;
    uint64_t peer_transaction_timeout_ms    = 500;
    bool     network_key_seeder             = false;
};

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

    void set_supported_integrity_algorithms(const u8_vec_t& algorithms);
    void set_supported_confidentiality_algorithms(const u8_vec_t& algorithms);

    void set_node_config(const node_config_s& config);

private:
    void add_beacon(beacon_ptr_t beacon);
    void rem_beacon(const port_ptr_t&);
    void rem_beacon(const sockaddr_t&);

    void on_beacon_timer_expired  (const beacon_ptr_t& beacon);
    void send_beacon              (const beacon_ptr_t& beacon);

    void on_port_in_queue_ready   (const port_ptr_t& port);
    void handle_pdu               (const port_ptr_t& port, const sockaddr_t& from, const sock_buff_t& pdu);
    void handle_beacon            (const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame);

    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::msg1& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::msg2& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::exchange_network_keys& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::link_info& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::link_report& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::route_announce& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::n2n_indication& msg);

    peer_ptr_t          peer_create(const node_id_t& node_id, const peer_public_key_s& public_key);
    peer_ptr_t          peer_lookup_or_create(const node_id_t& node_id, const port_ptr_t& port, const sockaddr_t& from);
    void                peer_update_link_activity(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& from, size_t size);
    bool                peer_supports_algorithm_pair(uint8_t integrity_algorithm, uint8_t confidentiality_algorithm) const;
    void                peer_abort_sender_security_procedure(const peer_ptr_t& peer);
    void                peer_send_msg2(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& to, uint8_t msg1_id, cum::status_e status, const dhke_kk* dhke);
    void                peer_schedule_sec_ctx_renewal_timer(const peer_ptr_t& peer, security_ctx_s& sec_ctx, uint64_t duration_s);
    void                peer_start_security_procedure(const peer_ptr_t& peer, bool force = false);
    uint8_t             peer_get_next_sec_ctx_id(const peer_ptr_t& peer);
    security_ctx_s&     peer_get_or_create_sec_ctx(const peer_ptr_t& peer, uint8_t id);
    security_ctx_t      peer_get_sec_ctx(const peer_ptr_t& peer, uint8_t id);
    void                peer_process_pending_transactions(const peer_ptr_t& peer);
    void                peer_retry_transaction(const peer_ptr_t& peer);
    void                peer_complete_transaction(const peer_ptr_t& peer, uint8_t id, int code);
    void                peer_expire_security_context(const peer_ptr_t& peer, uint8_t id, uint64_t creation_id);
    void                peer_schedule_check_activity();
    void                peer_cleanup(const peer_ptr_t& peer);
    uint64_t            now_s();
    void                reconfigure();

    cv_reactor_ptr_t            cv_reactor;
    node_config_s               config;

    bool                        is_initialized = false;
    downstream_identity_ptr_t   selected_downstream_identity;    
    peer_by_node_id_map         peers;
    ports_t                     ports;
    beacons_t                   beacons;
    sockaddrs_t                 static_peers;
    downstream_identities_t     downstream_identities;
    public_keys_t               public_keys;
    security_ctxs_t             network_security_contexts;
    u8_vec_t                    supported_integrity_algorithms;
    u8_vec_t                    supported_confidentiality_algorithms;

    // maintenance tasks
    timer_id_t                  check_peer_activity_timer_id;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_HPP
