#ifndef BFC_TUNNEL_NODE_HPP
#define BFC_TUNNEL_NODE_HPP

#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <bfc/sized_buffer.hpp>
#include <bfc/cv_reactor.hpp>

#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/security/dh.hpp>
#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/btprotocol.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>
#include <set>
#include <unordered_set>

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
    size_t                     mtu = 1500;
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
    uint64_t   expiration_time_s = 0;
    bool       is_expiring = false;
    timer_id_t timer_id;
};

using security_ctx_t  = std::optional<security_ctx_s>;
using security_ctxs_t = std::deque<security_ctx_s>;

struct network_key_ctx_s
{
    uint8_t          sec_ctx;
    uint64_t         priority;
    uint64_t         expiration_time_s;
    uint64_t         creation_id = 0;

    uint8_t          integrity_algorithm;
    key_t            integrity_key;
    uint8_t          confidentiality_algorithm;
    key_t            confidentiality_key;

    timer_id_t       expiration_timer_id;
};

using network_key_contexts_t = std::deque<network_key_ctx_s>;

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
    procedure_completion_cb_t completion_cb;

    dhke_kk             dhke;
};

using sec_proc_ctx_t = std::optional<security_procedure_ctx_s>;

struct network_key_acquisition_procedure_ctx_s
{
    uint8_t          request_id = 0;
    uint8_t          total_page = 0;
    unsigned         retry_count = 0;
    std::unordered_set<uint8_t> received_pages;
    cum::network_keys collected_keys;
    procedure_completion_cb_t completion_cb;
};

using net_key_acq_proc_ctx_t = std::optional<network_key_acquisition_procedure_ctx_s>;

struct peer_s
{
    node_id_t                 node_id = 0;
    std::optional<peer_public_key_s> public_key;

    // security
    security_ctxs_t           security_contexts;
    uint8_t                   sec_ctx_ctr = 0;
    uint32_t                  tx_sn = 0;

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
    net_key_acq_proc_ctx_t    net_key_acq_proc_ctx;

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

struct network_key_candidate_s
{
    node_id_t peer_id = 0;
    uint8_t   sec_ctx = 0;
    uint64_t  priority = 0;
    uint64_t  expiration_time_s = 0;
};

using network_key_candidates_t = std::list<network_key_candidate_s>;

struct network_security_procedure_ctx_s
{
    network_key_candidates_t  network_key_candidates;
    std::optional<timer_id_t> key_information_collect_timer_id;
    peer_weak_ptr_t           ongoing_acquisition_peer;
};

struct node_config_s
{
    uint64_t beacon_interval_ms               = 500;
    uint64_t check_peer_activity_interval_s   = 15;
    uint64_t peer_timeout_s                   = 15;
    uint64_t peer_security_ctx_timeout_s      = 120;
    uint64_t peer_transaction_timeout_ms      = 500;
    uint64_t network_security_query_timeout_ms = 500;
    uint64_t network_key_refresh_interval_s   = 30;
    bool     network_key_seeder               = false;
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
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::query_network_security& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::network_security_information& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::network_keys_request& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::network_keys_response& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::network_key_refresh& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::link_info& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::link_report& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::route_announce& msg);
    void handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::n2n_indication& msg);

    peer_ptr_t                    peer_create(const node_id_t& node_id);
    peer_ptr_t                    peer_lookup_or_create(const node_id_t& node_id, const port_ptr_t& port, const sockaddr_t& from);
    void                          peer_update_link_activity(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& from, size_t size);
    bool                          peer_supports_algorithm_pair(uint8_t integrity_algorithm, uint8_t confidentiality_algorithm) const;
    void                          peer_abort_sender_security_procedure(const peer_ptr_t& peer);
    void                          peer_finish_security_procedure(const peer_ptr_t& peer, int code);
    void                          peer_finish_network_key_acquisition_procedure(const peer_ptr_t& peer, int code);
    void                          peer_send_msg2(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& to, uint8_t msg1_id, cum::status_e status, const dhke_kk* dhke);
    void                          peer_schedule_sec_ctx_renewal_timer(const peer_ptr_t& peer, security_ctx_s& sec_ctx, uint64_t expiration_time_s);
    void                          peer_mark_sec_ctx_expiring(const peer_ptr_t& peer, uint8_t id, uint64_t creation_id);
    void                          peer_start_security_procedure(const peer_ptr_t& peer, procedure_completion_cb_t completion = {}, bool force = false);
    void                          peer_start_network_key_acquisition_procedure(const peer_ptr_t& peer, procedure_completion_cb_t completion = {});
    uint8_t                       peer_get_next_sec_ctx_id(const peer_ptr_t& peer);
    uint64_t                      peer_get_next_creation_id(const peer_ptr_t& peer) const;
    security_ctx_s&               peer_get_or_create_sec_ctx(const peer_ptr_t& peer, uint8_t id);
    security_ctx_s*               peer_find_sec_ctx(const peer_ptr_t& peer, uint8_t id);
    const security_ctx_s*         peer_find_sec_ctx(const peer_ptr_t& peer, uint8_t id) const;
    security_ctx_t                peer_get_sec_ctx(const peer_ptr_t& peer, uint8_t id);
    bool                          peer_sec_ctx_is_usable(const security_ctx_s& sec_ctx, uint64_t now) const;
    bool                          peer_has_usable_sec_ctx(const peer_ptr_t& peer) const;
    const security_ctx_s*         peer_select_sec_ctx(const peer_ptr_t& peer) const;
    void                          peer_process_pending_transactions(const peer_ptr_t& peer);
    void                          peer_retry_transaction(const peer_ptr_t& peer);
    void                          peer_complete_transaction(const peer_ptr_t& peer, uint8_t id, int code);
    void                          peer_expire_security_context(const peer_ptr_t& peer, uint8_t id, uint64_t creation_id);
    void                          peer_schedule_check_activity();
    void                          peer_cleanup(const peer_ptr_t& peer);
    template<typename Msg> bool   peer_send_message(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& to, const Msg& msg);


    uint64_t                      now_s() const;
    void                          reconfigure();

    bool                          network_has_keys() const;
    bool                          network_has_usable_key(uint8_t sec_ctx) const;
    void                          network_seed_keys();
    void                          network_install_key(const cum::network_key& key);
    void                          network_install_keys(const cum::network_keys& keys);
    cum::network_keys             network_export_keys() const;
    cum::network_key_informations network_export_key_informations() const;
    uint64_t                      network_get_next_creation_id() const;
    void                          network_expire_key(uint8_t sec_ctx, uint64_t creation_id);
    void                          network_acquire_keys();
    void                          network_start_security_query_procedure();
    void                          network_finish_security_query_procedure();
    void                          network_build_acquisition_candidates();
    void                          network_start_next_key_acquisition();
    void                          network_on_key_acquisition_complete(const peer_ptr_t& peer, int code);
    void                          network_schedule_key_refresh();
    void                          network_send_key_refresh();
    bool                          network_key_acquisition_in_progress() const;
    void                          network_send_keys_response(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& to, const cum::network_keys_response& msg);
    void                          network_send_security_information(const port_ptr_t& port, const sockaddr_t& to);
    template<typename Msg> void   network_send_public_message(const Msg& msg);
    bool                          network_accept_rx(const frame_const_t& frame, bfc::const_buffer_view pdu);
    bool                          network_try_send_frame(const port_ptr_t& port, const sockaddr_t& to, bfc::sized_buffer pdu, uint8_t sec_ctx);
    const network_key_ctx_s*      network_get_key_ctx(uint8_t sec_ctx) const;

    bool                          decode_beacon(const frame_const_t& frame) const;

    cv_reactor_ptr_t                 cv_reactor;
    node_config_s                    config;

    bool                             is_initialized = false;
    downstream_identity_ptr_t        selected_downstream_identity;    
    peer_by_node_id_map              peers;
    ports_t                          ports;
    beacons_t                        beacons;
    sockaddrs_t                      static_peers;
    downstream_identities_t          downstream_identities;
    public_keys_t                    public_keys;

    network_security_procedure_ctx_s network_security_procedure_ctx;
    network_key_contexts_t           network_security_contexts;
    u8_vec_t                         supported_integrity_algorithms;
    u8_vec_t                         supported_confidentiality_algorithms;

    // maintenance tasks
    timer_id_t                       check_peer_activity_timer_id;
    timer_id_t                       network_key_refresh_timer_id;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_HPP
