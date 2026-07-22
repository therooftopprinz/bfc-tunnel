#include "bfc/sized_buffer.hpp"
#include <bfc_tunnel/transport/transport_types.hpp>
#include <bfc_tunnel/node.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/utils/reactor_helper.hpp>
#include <bfc_tunnel/utils/logger.hpp>
#include <bfc_tunnel/utils/msg_utils.hpp>
#include <bfc_tunnel/utils/number_utils.hpp>
#include <bfc_tunnel/security/key_utils.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace bfc_tunnel
{

namespace
{

constexpr uint64_t k_sec_ctx_expiring_grace_s = 30;

bool key_preference_is_better(uint64_t candidate_priority, uint64_t candidate_expiration_s,
                              uint64_t existing_priority, uint64_t existing_expiration_s,
                              uint64_t now_s)
{
    const bool existing_expiring  = existing_expiration_s < now_s + k_sec_ctx_expiring_grace_s;
    const bool candidate_expiring = candidate_expiration_s < now_s + k_sec_ctx_expiring_grace_s;

    if (existing_expiring != candidate_expiring)
    {
        return !candidate_expiring;
    }

    if (candidate_expiration_s != existing_expiration_s)
    {
        return candidate_expiration_s < existing_expiration_s;
    }

    return candidate_priority > existing_priority;
}

cum::key_t to_cum_key(const key_t& key)
{
    cum::key_t out;
    out.assign(key.begin(), key.end());
    return out;
}

key_t from_cum_key(const cum::key_t& key)
{
    return key_t(key.begin(), key.end());
}

} // namespace

const char* to_string(peer_transaction_status_e status)
{
    switch (status)
    {
        case E_PEER_TRANSACTION_STATUS_OK:                          return "OK";
        case E_PEER_TRANSACTION_STATUS_MAX_RETRIES_REACHED:         return "MAX_RETRIES_REACHED";
        case E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS:             return "NOT_IN_PROGRESS";
        case E_PEER_TRANSACTION_STATUS_VERIFICATION_FAILED:         return "VERIFICATION_FAILED";
        case E_PEER_TRANSACTION_STATUS_UNSUPPORTED:                 return "UNSUPPORTED";
        case E_PEER_TRANSACTION_STATUS_NO_SUPPORTED_ALGORITHM_PAIR: return "NO_SUPPORTED_ALGORITHM_PAIR";
        case E_PEER_TRANSACTION_STATUS_PEER_TEARDOWN:               return "PEER_TEARDOWN";
        case E_PEER_TRANSACTION_STATUS_PEER_EXPIRED:                return "PEER_EXPIRED";
    }
    return "UNKNOWN";
}

node::node(cv_reactor_ptr_t cv_reactor)
    : cv_reactor(std::move(cv_reactor))
{}

node::~node()
{
    uninitialize();
}

void node::initialize()
{
    utils::dispatch_sync(*cv_reactor,
            [w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::initialize: Weak pointer expired!");
                    return;
                }

                t->is_initialized = true;
                if (t->config.network_key_seeder)
                {
                    t->network_seed_keys();
                }

                t->network_acquire_keys();
                t->network_schedule_key_refresh();

                t->peer_schedule_check_activity();
            }
        );
}

void node::uninitialize()
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t || !t->is_initialized)
            {
                return;
            }

            for (auto& port : t->ports)
            {
                t->cv_reactor->remove_read_rdy(port->transport->in);
            }
            
            for (auto& beacon : t->beacons)
            {
                t->cv_reactor->get_timer().cancel(beacon->beacon_timer_id);
            }

            t->ports.clear();
            t->beacons.clear();
            t->static_peers.clear();
            t->peers.clear();
            t->downstream_identities.clear();
            t->selected_downstream_identity = nullptr;

            for (auto& ctx : t->network_security_contexts)
            {
                t->cv_reactor->get_timer().cancel(ctx.expiration_timer_id);
            }
            t->network_security_contexts.clear();

            if (t->network_security_procedure_ctx.key_information_collect_timer_id)
            {
                t->cv_reactor->get_timer().cancel(*t->network_security_procedure_ctx.key_information_collect_timer_id);
            }
            t->network_security_procedure_ctx = {};
            t->cv_reactor->get_timer().cancel(t->network_key_refresh_timer_id);
            t->cv_reactor->get_timer().cancel(t->check_peer_activity_timer_id);

            t->is_initialized = false;
        });
}

void node::add_port(const port_ptr_t& port)
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this(), port]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::add_transport: Weak pointer expired!");
                return;
            }

            if (!t->is_initialized)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::add_port: Node is not initialized!", t.get());
                return;
            }
        
            if (!port)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::add_port: Port is null!", t.get());
                return;
            }

            t->ports.emplace_back(port);
            t->cv_reactor->add_read_rdy(port->transport->in, [w, port]()
                    {
                        auto t = w.lock();
                        if (!t)
                        {
                            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::add_port: Weak pointer expired!");
                            return;
                        }

                        t->on_port_in_queue_ready(port);
                    }
                );

            if (port->type == port_s::MULTICAST)
            {
                t->add_beacon(std::make_shared<beacon_s>(beacon_s{peer_address_s{port, sockaddr_none{}}, 500}));
            }
            else
            {
                for (auto& static_peer : t->static_peers)
                {
                    t->add_beacon(std::make_shared<beacon_s>(beacon_s{peer_address_s{port, static_peer}, 500}));
                }   
            }
        });
}

void node::rem_port(const port_ptr_t& port)
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this(), port]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::rem_port: Weak pointer expired!");
                return;
            }

            t->cv_reactor->remove_read_rdy(port->transport->in);
            t->ports.erase(std::remove(t->ports.begin(), t->ports.end(), port), t->ports.end());
            t->rem_beacon(port);

        });
}

void node::add_static_peer(const sockaddr_t& address)
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this(), address]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::add_static_peer: Weak pointer expired!");
                return;
            }

            t->static_peers.emplace_back(address);
            for (auto& port : t->ports)
            {
                if (port->type == port_s::UNICAST)
                {
                    t->add_beacon(std::make_shared<beacon_s>(beacon_s{peer_address_s{port, address}, 500}));
                }
            }
        });
}

void node::rem_static_peer(const sockaddr_t& address)
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this(), address]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::rem_static_peer: Weak pointer expired!");
                return;
            }

            t->static_peers.erase(
                std::remove_if(t->static_peers.begin(), t->static_peers.end(),
                    [&address](const sockaddr_t& peer) { return is_equal(peer, address); }),
                t->static_peers.end());
            t->rem_beacon(address);
        });
}

void node::set_supported_integrity_algorithms(const u8_vec_t& algorithms)
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this(), algorithms]()
        {
            auto t = w.lock();
            if (!t)
            {
                return;
            }

            t->supported_integrity_algorithms = algorithms;
        });
}

void node::set_supported_confidentiality_algorithms(const u8_vec_t& algorithms)
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this(), algorithms]()
        {
            auto t = w.lock();
            if (!t)
            {
                return;
            }

            t->supported_confidentiality_algorithms = algorithms;
        });
}

void node::set_node_config(const node_config_s& config)
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this(), config]()
        {
            auto t = w.lock();
            if (!t)
            {
                return;
            }

            t->config = config;
            if (t->is_initialized)
            {
                t->reconfigure();
                return;
            }
        });
}

void node::add_beacon(beacon_ptr_t beacon)
{
    beacons.emplace_back(beacon);
    on_beacon_timer_expired(beacon);
}

void node::rem_beacon(const port_ptr_t& port)
{
    std::list<std::vector<beacon_ptr_t>::iterator> to_remove;
    for (auto it = beacons.begin(); it != beacons.end(); ++it)
    {
        if ((*it)->peer.port == port)
        {
            cv_reactor->get_timer().cancel((*it)->beacon_timer_id);
            to_remove.emplace_front(it);
        }
    }

    for (auto& it : to_remove)
    {
        beacons.erase(it);
    }
}

void node::rem_beacon(const sockaddr_t& address)
{
    std::list<std::vector<beacon_ptr_t>::iterator> to_remove;
    for (auto it = beacons.begin(); it != beacons.end(); ++it)
    {
        if (is_equal((*it)->peer.address, address))
        {
            cv_reactor->get_timer().cancel((*it)->beacon_timer_id);
            to_remove.emplace_front(it);
        }
    }

    for (auto& it : to_remove)
    {
        beacons.erase(it);
    }
}

void node::on_beacon_timer_expired(const beacon_ptr_t& beacon)
{
    beacon->beacon_timer_id = cv_reactor->get_timer().wait_ms(beacon->beacon_interval_ms,
        [w = weak_from_this(), beacon]()
        {
            auto t = w.lock();
            if (!t)
            {
                return;
            }

            t->send_beacon(beacon);
            t->on_beacon_timer_expired(beacon);
        });
}

void node::send_beacon(const beacon_ptr_t& beacon)
{
    if (!selected_downstream_identity)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::send_beacon: no downstream identity!", this);
        return;
    }

    bfc::sized_buffer pdu(1024*65);

    auto frame = prepare_frame(pdu);

    frame.set_ttl(0);
    frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PUBLIC);
    frame.set_sec_ctx(k_sec_ctx_none);
    frame.set_mac_size_units(0);
    frame.set_sn(0);
    frame.set_src(selected_downstream_identity->node_id);
    frame.set_dst(0xFFFFFFFF);
    frame.set_payload_type(payload_type_e::E_PAYLOAD_TYPE_BEACON);

    cum::beacon msg;
    msg.flags = 0;

    if (!encode_payload(frame, msg))
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::send_beacon: Failed to encode beacon!", this);
        return;
    }

    pdu.resize(frame.get_size());
    beacon->peer.port->transport->out.push(transport_out_t{transport_data_s{0, beacon->peer.address, std::move(pdu)}});
}

void node::handle_pdu(const port_ptr_t& port, const sockaddr_t& from, const sock_buff_t& pdu)
{
    if (pdu.empty())
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_pdu: Empty PDU!", this);
        return;
    }

    auto frame = to_frame(pdu.data(), pdu.size());

    if (!validate_frame(frame))
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_pdu: Invalid frame!", this);
        return;
    }

    const auto frame_type = frame.get_frame_type();
    if (frame_type == E_FRAME_TYPE_NETWORK || frame_type == E_FRAME_TYPE_NETWORK_OVER_PEER)
    {
        if (!network_accept_rx(frame, bfc::const_buffer_view(pdu.data(), pdu.size())))
        {
            return;
        }
    }

    if (frame.get_payload_type() == E_PAYLOAD_TYPE_BEACON)
    {
        handle_beacon(port, from, frame);
        return;
    }
    else if (frame.get_payload_type() == E_PAYLOAD_TYPE_TUNNEL_DATA)
    {
        return;
    }

    const auto payload = frame_payload_view(frame, pdu);
    if (payload.empty())
    {
        return;
    }

    try
    {
        cum::per_codec_ctx ctx(
            const_cast<std::byte*>(payload.data()),
            payload.size()
        );

        switch (frame.get_payload_type())
        {
        case E_PAYLOAD_TYPE_MSG1:
        {
            cum::msg1 msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_MSG2:
        {
            cum::msg2 msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_QUERY_NETWORK_SECURITY:
        {
            cum::query_network_security msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_NETWORK_SECURITY_INFORMATION:
        {
            cum::network_security_information msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_NETWORK_KEYS_REQUEST:
        {
            cum::network_keys_request msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_NETWORK_KEYS_RESPONSE:
        {
            cum::network_keys_response msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_NETWORK_KEY_REFRESH:
        {
            cum::network_key_refresh msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_LINK_INFO:
        {
            cum::link_info msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_LINK_REPORT:
        {
            cum::link_report msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_ROUTE_ANNOUNCE:
        {
            cum::route_announce msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_N2N_INDICATION:
        {
            cum::n2n_indication msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, from, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_TUNNEL_DATA:
            log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_pdu: Tunnel data!", this);
            break;
        default:
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_pdu: Unknown payload type %u!", this,
                static_cast<unsigned>(frame.get_payload_type()));
            break;
        }
    }
    catch (const std::exception& e)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_pdu: PER decode failed: %s", this, e.what());
    }
}

void node::on_port_in_queue_ready(const port_ptr_t& port)
{
    auto batch = port->transport->in.pop();
    if (batch.empty())
    {
        return;
    }

    for (auto& item : batch)
    {
        handle_pdu(port, item.address, item.data);
    }
}

void node::handle_beacon(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame)
{
    if (!decode_beacon(frame))
    {
        return;
    }

    const node_id_t peer_node_id = frame.get_src();
    auto peer_it = peers.find(peer_node_id);
    if (peers.end() == peer_it)
    {
        auto peer = peer_create(peer_node_id);
        auto public_key_it = public_keys.find(peer_node_id);
        if (public_keys.end() != public_key_it)
        {
            peer->public_key = public_key_it->second;
        }
        peer->preferred_peer_address = peer_address_s{port, from};
        peer_update_link_activity(peer, port, from, frame.get_size());

        return;
    }

    auto peer = peer_it->second;
    if (!peer->public_key)
    {
        auto public_key_it = public_keys.find(peer_node_id);
        if (public_keys.end() != public_key_it)
        {
            peer->public_key = public_key_it->second;
        }
    }

    peer_update_link_activity(peer, port, from, frame.get_size());
}

void node::handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::msg1& msg)
{
    if (!selected_downstream_identity)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(msg1): no downstream identity!", this);
        return;
    }

    const node_id_t peer_node_id = frame.get_src();
    auto peer = peer_lookup_or_create(peer_node_id, port, from);
    if (!peer)
    {
        return;
    }

    peer_update_link_activity(peer, port, from, frame.get_size());
    peer->preferred_peer_address = peer_address_s{port, from};

    if (!peer->public_key)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(msg1): no public key for peer %04x!", this, peer_node_id);
        peer_send_msg2(peer, port, from, msg.id, cum::status_e::UKNOWN_PEER, nullptr);
        return;
    }

    if (peer->sec_proc_ctx)
    {
        if (msg.priority > peer->sec_proc_ctx->msg1.priority)
        {
            peer_abort_sender_security_procedure(peer);
        }
        else if (msg.priority < peer->sec_proc_ctx->msg1.priority)
        {
            return;
        }
        else
        {
            peer_abort_sender_security_procedure(peer);
            peer_start_security_procedure(peer, {}, true);
            return;
        }
    }

    if (msg.sec_ctx >= 16)
    {
        peer_send_msg2(peer, port, from, msg.id, cum::status_e::VERIFICATION_FAILED, nullptr);
        return;
    }

    if (msg.expiration_time_s <= now_s())
    {
        peer_send_msg2(peer, port, from, msg.id, cum::status_e::VERIFICATION_FAILED, nullptr);
        return;
    }

    const auto key_type = static_cast<dh_key_type_e>(msg.dh_key_type);
    if (key_type != E_DHKT_X25519 || !make_dh_backend(key_type))
    {
        peer_send_msg2(peer, port, from, msg.id, cum::status_e::VERIFICATION_FAILED, nullptr);
        return;
    }

    if (!peer_supports_algorithm_pair(msg.integrity_algorithm, msg.confidentiality_algorithm))
    {
        peer_send_msg2(peer, port, from, msg.id, cum::status_e::UNSUPPORTED, nullptr);
        return;
    }

    dhke_kk dhke(make_dh_backend(key_type));
    dhke.setup_handshake(selected_downstream_identity->private_key, peer->public_key->public_key, false);
    dhke.set_peer_ephemeral_public(msg.ephemeral);
    key_t own_ephemeral;
    dhke.get_own_ephemeral_public(own_ephemeral);

    auto creation_id = peer_get_next_creation_id(peer);
    auto& sec_ctx = peer_get_or_create_sec_ctx(peer, msg.sec_ctx);
    peer->supported_algorithm_pairs.emplace(msg.integrity_algorithm, msg.confidentiality_algorithm);
    sec_ctx.creation_id               = creation_id;
    sec_ctx.confidentiality_algorithm = msg.confidentiality_algorithm;
    sec_ctx.integrity_algorithm       = msg.integrity_algorithm;
    sec_ctx.integrity_key             = dhke.derive_integrity_key(sec_ctx.integrity_algorithm);
    sec_ctx.confidentiality_key       = dhke.derive_confidentiality_key(sec_ctx.confidentiality_algorithm);
    sec_ctx.is_expiring               = false;
    peer_schedule_sec_ctx_renewal_timer(peer, sec_ctx, msg.expiration_time_s);

    peer_send_msg2(peer, port, from, msg.id, cum::status_e::OK, &dhke);
}

void node::handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::msg2& msg)
{
    const node_id_t peer_node_id = frame.get_src();
    auto peer_it = peers.find(peer_node_id);
    if (peers.end() == peer_it)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(msg2): unexpected peer %04x!", this, peer_node_id);
        return;
    }

    auto peer = peer_it->second;
    peer_update_link_activity(peer, port, from, frame.get_size());

    if (!peer->sec_proc_ctx)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(msg2): no security procedure in progress for peer %04x!", this, peer_node_id);
        peer_complete_transaction(peer, msg.id, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
        return;
    }

    auto& sec_proc_ctx = peer->sec_proc_ctx.value();

    if (!peer->public_key)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(msg2): no public key for peer %04x!", this, peer_node_id);
        peer_complete_transaction(peer, msg.id, E_PEER_TRANSACTION_STATUS_VERIFICATION_FAILED);
        return;
    }

    if (msg.status == cum::status_e::UNSUPPORTED)
    {
        procedure_completion_cb_t completion;
        if (peer->sec_proc_ctx)
        {
            completion = peer->sec_proc_ctx->completion_cb;
        }

        peer->supported_algorithm_pairs.erase(sec_proc_ctx.algo_pair_used);
        peer->test_algorithm_pairs.erase(sec_proc_ctx.algo_pair_used);
        peer_complete_transaction(peer, msg.id, E_PEER_TRANSACTION_STATUS_UNSUPPORTED);
        peer_start_security_procedure(peer, std::move(completion), true);
        return;
    }

    if (msg.status == cum::status_e::VERIFICATION_FAILED)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(msg2): verification failed for peer %04x!", this, peer_node_id);
        peer_complete_transaction(peer, msg.id, E_PEER_TRANSACTION_STATUS_VERIFICATION_FAILED);
        public_keys.erase(peer->node_id);
        peer->public_key.reset();
        return;
    }

    if (msg.status == cum::status_e::OK)
    {
        peer->sec_proc_ctx->dhke.set_peer_ephemeral_public(msg.ephemeral);

        auto& sec_ctx = peer_get_or_create_sec_ctx(peer, sec_proc_ctx.id);
        peer->supported_algorithm_pairs.emplace(sec_proc_ctx.msg1.integrity_algorithm, sec_proc_ctx.msg1.confidentiality_algorithm);
        sec_ctx.creation_id                = sec_proc_ctx.creation_id;
        sec_ctx.confidentiality_algorithm  = sec_proc_ctx.msg1.confidentiality_algorithm;
        sec_ctx.integrity_algorithm        = sec_proc_ctx.msg1.integrity_algorithm;
        sec_ctx.integrity_key              = sec_proc_ctx.dhke.derive_integrity_key(sec_ctx.integrity_algorithm);
        sec_ctx.confidentiality_key        = sec_proc_ctx.dhke.derive_confidentiality_key(sec_ctx.confidentiality_algorithm);
        sec_ctx.is_expiring                = false;
        peer_schedule_sec_ctx_renewal_timer(peer, sec_ctx, sec_proc_ctx.msg1.expiration_time_s);
    }

    peer_complete_transaction(peer, msg.id, E_PEER_TRANSACTION_STATUS_OK);
}

void node::handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::query_network_security& /*msg*/)
{
    if (!selected_downstream_identity)
    {
        return;
    }

    const node_id_t peer_node_id = frame.get_src();
    auto peer = peer_lookup_or_create(peer_node_id, port, from);
    if (peer)
    {
        peer_update_link_activity(peer, port, from, frame.get_size());
    }

    network_send_security_information(port, from);
}

void node::handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::network_security_information& msg)
{
    if (!network_security_procedure_ctx.key_information_collect_timer_id)
    {
        return;
    }

    auto& info_ctx = network_security_procedure_ctx;

    const node_id_t peer_node_id = frame.get_src();
    auto peer = peer_lookup_or_create(peer_node_id, port, from);
    if (!peer)
    {
        return;
    }

    peer_update_link_activity(peer, port, from, frame.get_size());
    peer->preferred_peer_address = peer_address_s{port, from};

    for (const auto& info : msg.informations)
    {
        auto it = std::find_if(info_ctx.network_key_candidates.begin(), info_ctx.network_key_candidates.end(),
            [peer_node_id, sec_ctx = info.sec_ctx](const network_key_candidate_s& entry)
            {
                return entry.peer_id == peer_node_id && entry.sec_ctx == sec_ctx;
            });
        if (it != info_ctx.network_key_candidates.end())
        {
            it->priority = info.priority;
            it->expiration_time_s = info.expiration_time_s;
        }
        else
        {
            info_ctx.network_key_candidates.push_back(network_key_candidate_s{
                peer_node_id, info.sec_ctx, info.priority, info.expiration_time_s});
        }
    }
}

void node::handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::network_keys_request& msg)
{
    if (!selected_downstream_identity)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(network_keys_request): no downstream identity!", this);
        return;
    }

    const node_id_t peer_node_id = frame.get_src();
    auto peer = peer_lookup_or_create(peer_node_id, port, from);
    if (!peer)
    {
        return;
    }

    peer_update_link_activity(peer, port, from, frame.get_size());
    peer->preferred_peer_address = peer_address_s{port, from};

    if (network_security_contexts.empty())
    {
        log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message(network_keys_request): no network keys to share!", this);
        return;
    }

    cum::network_keys_response response;
    response.id = msg.id;
    response.current_page = 0;
    response.total_page = 1;
    response.keys = network_export_keys();

    network_send_keys_response(peer, port, from, response);
}

void node::handle_btp_message(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame, cum::network_keys_response& msg)
{
    const node_id_t peer_node_id = frame.get_src();
    auto peer = peer_lookup_or_create(peer_node_id, port, from);
    if (!peer || !peer->net_key_acq_proc_ctx)
    {
        log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message(network_keys_response): no acquisition in progress for peer %04x!", this, peer_node_id);
        return;
    }

    auto& acq_ctx = peer->net_key_acq_proc_ctx.value();

    if (msg.id != acq_ctx.request_id)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(network_keys_response): request id mismatch!", this);
        return;
    }

    peer_update_link_activity(peer, port, from, frame.get_size());

    acq_ctx.total_page = msg.total_page;
    acq_ctx.received_pages.insert(msg.current_page);
    for (const auto& key : msg.keys)
    {
        acq_ctx.collected_keys.push_back(key);
    }

    if (acq_ctx.received_pages.size() < msg.total_page)
    {
        return;
    }

    network_install_keys(acq_ctx.collected_keys);
    network_schedule_key_refresh();

    if (!peer->current_transaction)
    {
        peer_finish_network_key_acquisition_procedure(peer, E_PEER_TRANSACTION_STATUS_OK);
        return;
    }

    peer_complete_transaction(peer, peer->current_transaction->id, E_PEER_TRANSACTION_STATUS_OK);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const sockaddr_t& /*from*/, const frame_const_t& /*frame*/, cum::network_key_refresh& msg)
{
    network_install_keys(msg.keys);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const sockaddr_t& /*from*/, const frame_const_t& /*frame*/, cum::link_info& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Link info!", this);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const sockaddr_t& /*from*/, const frame_const_t& /*frame*/, cum::link_report& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Link report!", this);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const sockaddr_t& /*from*/, const frame_const_t& /*frame*/, cum::route_announce& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Route announce!", this);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const sockaddr_t& /*from*/, const frame_const_t& /*frame*/, cum::n2n_indication& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: N2N indication!", this);
}

void node::peer_update_link_activity(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& from, size_t size)
{
    peer->last_activity_time_s = now_s();

    auto& links = port->type == port_s::UNICAST ? peer->unicast_links : peer->multicast_links;

    auto link_it = std::find_if(links.begin(), links.end(), 
        [&from, &port](const std::pair<peer_address_s, peer_link_ctx_s>& link) 
        {
            return is_equal(link.first.address, from) && link.first.port == port;
        });

    if (links.end() == link_it)
    {
        links.emplace_back(std::make_pair(peer_address_s{port, from}, peer_link_ctx_s{}));
        auto& link = links.back();
        link.second.last_activity_time_s = now_s();
        link.second.recv_pkt++;
        link.second.recv_byt += size;
    }
    else
    {
        link_it->second.last_activity_time_s = now_s();
        link_it->second.recv_pkt++;
        link_it->second.recv_byt += size;
    }
}

peer_ptr_t node::peer_create(const node_id_t& node_id)
{
    auto peer = std::make_shared<peer_s>();
    peer->node_id = node_id;
    peer->test_algorithm_pairs = get_pairs(supported_integrity_algorithms, supported_confidentiality_algorithms);
    peers.emplace(node_id, peer);
    return peer;
}

uint8_t node::peer_get_next_sec_ctx_id(const peer_ptr_t& peer)
{
    std::set<uint8_t> sec_ctx_ids;
    for (uint8_t i = 0; i < 16; i++)
    {
        sec_ctx_ids.insert(i);
    }

    for (auto& sec_ctx : peer->security_contexts)
    {
        sec_ctx_ids.erase(sec_ctx.id);
    }

    if (sec_ctx_ids.empty())
    {
        return 0;
    }

    return *sec_ctx_ids.begin();
}

uint64_t node::peer_get_next_creation_id(const peer_ptr_t& peer) const
{
    std::set<uint64_t> used;
    for (const auto& sec_ctx : peer->security_contexts)
    {
        used.insert(sec_ctx.creation_id);
    }
    if (peer->sec_proc_ctx)
    {
        used.insert(peer->sec_proc_ctx->creation_id);
    }

    uint64_t id = 0;
    while (used.count(id))
    {
        ++id;
    }
    return id;
}

security_ctx_s& node::peer_get_or_create_sec_ctx(const peer_ptr_t& peer, uint8_t id)
{
    auto ctx_it = std::find_if(peer->security_contexts.begin(), peer->security_contexts.end(), [&id](const security_ctx_s& ctx) { return ctx.id == id; });
    if (peer->security_contexts.end() == ctx_it)
    {
        security_ctx_s ctx;
        ctx.id = id;
        peer->security_contexts.emplace_back(std::move(ctx));
        return peer->security_contexts.back();
    }

    return *ctx_it;
}

security_ctx_s* node::peer_find_sec_ctx(const peer_ptr_t& peer, uint8_t id)
{
    auto ctx_it = std::find_if(peer->security_contexts.begin(), peer->security_contexts.end(),
        [&id](const security_ctx_s& ctx) { return ctx.id == id; });
    if (peer->security_contexts.end() == ctx_it)
    {
        return nullptr;
    }
    return &(*ctx_it);
}

const security_ctx_s* node::peer_find_sec_ctx(const peer_ptr_t& peer, uint8_t id) const
{
    auto ctx_it = std::find_if(peer->security_contexts.begin(), peer->security_contexts.end(),
        [&id](const security_ctx_s& ctx) { return ctx.id == id; });
    if (peer->security_contexts.end() == ctx_it)
    {
        return nullptr;
    }
    return &(*ctx_it);
}

security_ctx_t node::peer_get_sec_ctx(const peer_ptr_t& peer, uint8_t id)
{
    const security_ctx_s* ctx = peer_find_sec_ctx(peer, id);
    if (!ctx)
    {
        return std::nullopt;
    }
    return *ctx;
}

bool node::peer_sec_ctx_is_usable(const security_ctx_s& sec_ctx, uint64_t now) const
{
    return !sec_ctx.is_expiring && sec_ctx.expiration_time_s > now + k_sec_ctx_expiring_grace_s;
}

bool node::peer_has_usable_sec_ctx(const peer_ptr_t& peer) const
{
    const uint64_t now = now_s();
    for (const auto& sec_ctx : peer->security_contexts)
    {
        if (peer_sec_ctx_is_usable(sec_ctx, now))
        {
            return true;
        }
    }
    return false;
}

const security_ctx_s* node::peer_select_sec_ctx(const peer_ptr_t& peer) const
{
    const uint64_t now = now_s();
    const security_ctx_s* fallback = nullptr;

    for (const auto& sec_ctx : peer->security_contexts)
    {
        if (sec_ctx.expiration_time_s <= now)
        {
            continue;
        }

        if (peer_sec_ctx_is_usable(sec_ctx, now))
        {
            return &sec_ctx;
        }

        if (!fallback)
        {
            fallback = &sec_ctx;
        }
    }

    return fallback;
}

peer_ptr_t node::peer_lookup_or_create(const node_id_t& node_id, const port_ptr_t& port, const sockaddr_t& from)
{
    auto peer_it = peers.find(node_id);
    if (peers.end() != peer_it)
    {
        auto peer = peer_it->second;
        peer->preferred_peer_address = peer_address_s{port, from};
        if (!peer->public_key)
        {
            auto public_key_it = public_keys.find(node_id);
            if (public_keys.end() != public_key_it)
            {
                peer->public_key = public_key_it->second;
            }
        }
        return peer;
    }

    auto peer = peer_create(node_id);
    auto public_key_it = public_keys.find(node_id);
    if (public_keys.end() != public_key_it)
    {
        peer->public_key = public_key_it->second;
    }
    peer->preferred_peer_address = peer_address_s{port, from};
    return peer;
}

bool node::peer_supports_algorithm_pair(uint8_t integrity_algorithm, uint8_t confidentiality_algorithm) const
{
    return std::find(supported_integrity_algorithms.begin(), supported_integrity_algorithms.end(), integrity_algorithm) != supported_integrity_algorithms.end()
        && std::find(supported_confidentiality_algorithms.begin(), supported_confidentiality_algorithms.end(), confidentiality_algorithm) != supported_confidentiality_algorithms.end();
}

void node::peer_abort_sender_security_procedure(const peer_ptr_t& peer)
{
    cv_reactor->get_timer().cancel(peer->transaction_timer_id);
    peer->current_transaction.reset();
    peer_finish_security_procedure(peer, E_PEER_TRANSACTION_STATUS_PEER_TEARDOWN);
}

void node::peer_finish_security_procedure(const peer_ptr_t& peer, int code)
{
    if (!peer->sec_proc_ctx)
    {
        return;
    }

    auto completion = std::move(peer->sec_proc_ctx->completion_cb);
    peer->sec_proc_ctx.reset();

    if (completion)
    {
        completion(*this, peer, code);
    }
}

void node::peer_send_msg2(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& to, uint8_t msg1_id, cum::status_e status, const dhke_kk* dhke)
{
    if (!peer->public_key)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_send_msg2: no public key for peer %04x!", this, peer->node_id);
        return;
    }

    bfc::sized_buffer pdu(1024 * 65);
    auto frame = prepare_frame(pdu);
    frame.set_ttl(0);
    frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PEER);
    frame.set_sec_ctx(k_sec_ctx_none);
    frame.set_mac_size_units(0);
    frame.set_src(selected_downstream_identity->node_id);
    frame.set_dst(peer->node_id);
    frame.set_ts(now_s());
    frame.set_sn(peer->tx_sn++);

    cum::msg2 msg2;
    msg2.id = msg1_id;
    msg2.status = status;
    if (status == cum::status_e::OK && dhke)
    {
        msg2.ephemeral = dhke->own_ephemeral_public();
    }

    if (!encode_payload(frame, msg2))
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_send_msg2: Failed to encode MSG2 for peer %04x!", this, peer->node_id);
        return;
    }

    pdu.resize(frame.get_size());
    port->transport->out.push(transport_out_t{transport_data_s{0, to, std::move(pdu)}});
}

void node::peer_schedule_sec_ctx_renewal_timer(const peer_ptr_t& peer, security_ctx_s& sec_ctx, uint64_t expiration_time_s)
{
    cv_reactor->get_timer().cancel(sec_ctx.timer_id);

    sec_ctx.expiration_time_s = expiration_time_s;
    const uint64_t now = now_s();
    const uint64_t remaining_s = expiration_time_s > now ? expiration_time_s - now : 0;
    const uint64_t wait_s = remaining_s > k_sec_ctx_expiring_grace_s
        ? remaining_s - k_sec_ctx_expiring_grace_s
        : 0;

    sec_ctx.timer_id = cv_reactor->get_timer().wait_ms(wait_s * 1000,
        [w = weak_from_this(),
        peerw = peer_weak_ptr_t(peer),
        id = sec_ctx.id,
        creation_id = sec_ctx.creation_id]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_schedule_sec_ctx_renewal_timer: Node expired!", t.get());
                return;
            }

            auto peer = peerw.lock();
            if (!peer)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_schedule_sec_ctx_renewal_timer: Peer expired!", t.get());
                return;
            }

            t->peer_mark_sec_ctx_expiring(peer, id, creation_id);
        });
}

void node::peer_mark_sec_ctx_expiring(const peer_ptr_t& peer, uint8_t id, uint64_t creation_id)
{
    security_ctx_s* sec_ctx = peer_find_sec_ctx(peer, id);
    if (!sec_ctx)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_mark_sec_ctx_expiring: Security context not found for peer %04x!", this, peer->node_id);
        return;
    }

    if (creation_id != sec_ctx->creation_id)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_mark_sec_ctx_expiring: Creation ID mismatch for security context %u!", this, id);
        return;
    }

    if (sec_ctx->is_expiring)
    {
        return;
    }

    sec_ctx->is_expiring = true;

    const uint64_t now = now_s();
    const uint64_t remaining_s = sec_ctx->expiration_time_s > now ? sec_ctx->expiration_time_s - now : 0;
    cv_reactor->get_timer().cancel(sec_ctx->timer_id);
    sec_ctx->timer_id = cv_reactor->get_timer().wait_ms(remaining_s * 1000,
        [w = weak_from_this(), peerw = peer_weak_ptr_t(peer), id, creation_id]()
        {
            auto t = w.lock();
            if (!t)
            {
                return;
            }

            auto peer = peerw.lock();
            if (!peer)
            {
                return;
            }

            t->peer_expire_security_context(peer, id, creation_id);
        });

    peer_start_security_procedure(peer);
}

void node::peer_finish_network_key_acquisition_procedure(const peer_ptr_t& peer, int code)
{
    if (!peer->net_key_acq_proc_ctx)
    {
        return;
    }

    std::unordered_set<uint8_t> acquired_sec_ctxs;
    if (code == E_PEER_TRANSACTION_STATUS_OK)
    {
        for (const auto& key : peer->net_key_acq_proc_ctx->collected_keys)
        {
            acquired_sec_ctxs.insert(key.sec_ctx);
        }
    }

    auto completion = std::move(peer->net_key_acq_proc_ctx->completion_cb);
    peer->net_key_acq_proc_ctx.reset();

    if (!acquired_sec_ctxs.empty())
    {
        network_security_procedure_ctx.network_key_candidates.remove_if(
            [&acquired_sec_ctxs](const network_key_candidate_s& candidate)
            {
                return acquired_sec_ctxs.find(candidate.sec_ctx) != acquired_sec_ctxs.end();
            });
    }

    if (completion)
    {
        completion(*this, peer, code);
    }
}

void node::peer_start_security_procedure(const peer_ptr_t& peer, procedure_completion_cb_t completion, bool force)
{
    if (!peer->public_key)
    {
        if (completion)
        {
            completion(*this, peer, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
        }
        return;
    }

    if (!force && peer_has_usable_sec_ctx(peer))
    {
        if (completion)
        {
            completion(*this, peer, E_PEER_TRANSACTION_STATUS_OK);
        }
        return;
    }

    if (peer->sec_proc_ctx)
    {
        return;
    }

    uint8_t ctx_id = peer_get_next_sec_ctx_id(peer);
    auto creation_id = peer_get_next_creation_id(peer);

    dhke_kk dhke(make_dh_backend(peer->public_key->key_type));
    if (selected_downstream_identity)
    {
        dhke.setup_handshake(selected_downstream_identity->private_key, peer->public_key->public_key, true);
    }

    peer->sec_proc_ctx.emplace(
        security_procedure_ctx_s{
            ctx_id,
            security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_QUEUED,
            0,
            {},
            {},
            {},
            {},
            creation_id,
            std::move(completion),
            std::move(dhke)});

    if (peer->sec_proc_ctx->state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_ONGOING ||
        peer->sec_proc_ctx->state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_EXPIRED)
    {   
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_security_procedure: Security procedure already in progress for peer %04x!", this, peer->node_id);
        return;
    }

    transaction_cb_t do_cb = [](node& t, const peer_ptr_t& peer, uint8_t id) -> void
    {
        if (!peer->sec_proc_ctx)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_security_procedure: Security procedure not in progress for peer %04x!", &t, peer->node_id);
            t.peer_complete_transaction(peer, id, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
            return;
        }

        auto& sec_proc_ctx = peer->sec_proc_ctx.value();

        if (sec_proc_ctx.state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_QUEUED)
        {
            sec_proc_ctx.pdu = bfc::sized_buffer(1024*65);
            sec_proc_ctx.frame = prepare_frame(sec_proc_ctx.pdu);
            sec_proc_ctx.frame.set_ttl(0);
            sec_proc_ctx.frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PEER);
            sec_proc_ctx.frame.set_sec_ctx(k_sec_ctx_none);
            sec_proc_ctx.frame.set_mac_size_units(0);
            sec_proc_ctx.frame.set_src(t.selected_downstream_identity->node_id);
            sec_proc_ctx.frame.set_dst(peer->node_id);

            sec_proc_ctx.msg1.id          = id;
            sec_proc_ctx.msg1.sec_ctx     = sec_proc_ctx.id;
            sec_proc_ctx.msg1.dh_key_type = static_cast<uint8_t>(peer->public_key->key_type);
            sec_proc_ctx.dhke.get_own_ephemeral_public(sec_proc_ctx.msg1.ephemeral);
            sec_proc_ctx.msg1.expiration_time_s = t.now_s() + t.config.peer_security_ctx_timeout_s;
            sec_proc_ctx.msg1.priority   = random_u64();

            uint8_t integrity_algorithm       = 0;
            uint8_t confidentiality_algorithm = 0;
    
            if (!peer->supported_algorithm_pairs.empty())
            {
                auto& pair = *peer->supported_algorithm_pairs.rbegin();
                integrity_algorithm = pair.first;
                confidentiality_algorithm = pair.second;
            }
            else if (!peer->test_algorithm_pairs.empty())
            {
                auto& pair_it = *peer->test_algorithm_pairs.rbegin();
                integrity_algorithm = pair_it.first;
                confidentiality_algorithm = pair_it.second;
                peer->test_algorithm_pairs.erase(pair_it);
            }
            else
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_security_procedure: No supported or test algorithm pairs for peer %04x!", &t, peer->node_id);
                t.peer_complete_transaction(peer, id, E_PEER_TRANSACTION_STATUS_NO_SUPPORTED_ALGORITHM_PAIR);
                return;
            }

            sec_proc_ctx.algo_pair_used = std::make_pair(integrity_algorithm, confidentiality_algorithm);
    
            sec_proc_ctx.msg1.integrity_algorithm = integrity_algorithm;
            sec_proc_ctx.msg1.confidentiality_algorithm = confidentiality_algorithm;

            if (!encode_payload(sec_proc_ctx.frame, sec_proc_ctx.msg1))
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_security_procedure: Failed to encode MSG1 for peer %u!", &t, peer->node_id);
                t.peer_complete_transaction(peer, id, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
                return;
            }
            
            sec_proc_ctx.pdu.resize(sec_proc_ctx.frame.get_size());
        }

        sec_proc_ctx.frame.set_ts(t.now_s());
        sec_proc_ctx.frame.set_sn(peer->tx_sn++);

        auto pdu = bfc::sized_buffer(sec_proc_ctx.pdu.size());
        std::memcpy(pdu.data(), sec_proc_ctx.pdu.data(), pdu.size());
        peer->preferred_peer_address.port->transport->out.push(transport_out_t{transport_data_s{0, peer->preferred_peer_address.address, std::move(pdu)}});

        sec_proc_ctx.state = security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_ONGOING;
        sec_proc_ctx.retry_count++;
    };

    expiration_cb_t expiration_cb = [](node& t, const peer_ptr_t& peer, uint8_t id) -> void
    {
        if (!peer->sec_proc_ctx)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::expiration_cb_t: Security procedure not in progress for peer %04x!", &t, peer->node_id);
            t.peer_complete_transaction(peer, 0, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
            return;
        }

        if (id != peer->sec_proc_ctx->msg1.id || id != peer->current_transaction->id)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::expiration_cb_t: Unexpected transaction ID %u for peer %04x!", &t, id, peer->node_id);
            return;
        }

        auto& sec_proc_ctx = peer->sec_proc_ctx;

        if (sec_proc_ctx->state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_ONGOING && sec_proc_ctx->retry_count >= 3)
        {
            t.peer_complete_transaction(peer, 0, E_PEER_TRANSACTION_STATUS_MAX_RETRIES_REACHED);
            return;
        }

        sec_proc_ctx->state = security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_EXPIRED;
        t.peer_retry_transaction(peer);
    };

    completion_cb_t done_cb = [](node& t, const peer_ptr_t& peer, uint8_t id, int code) -> void
    {
        log(*g_logger, E_LOG_BIT_INFO, "node[%p]::completion_cb_t: Transaction %u completed with status %s for peer %04x!", &t, id, to_string(static_cast<peer_transaction_status_e>(code)), peer->node_id);

        t.peer_finish_security_procedure(peer, code);
    };

    peer->pending_transactions.emplace_back(peer_transaction_s{peer->trid_ctr++, std::move(do_cb), done_cb, std::move(expiration_cb)});
    peer_process_pending_transactions(peer);
}

void node::peer_process_pending_transactions(const peer_ptr_t& peer)
{
    if (peer->current_transaction)
    {
        return;
    }

    if (peer->pending_transactions.empty())
    {
        return;
    }

    auto& transaction = peer->pending_transactions.front();
    peer->current_transaction = std::move(transaction);
    peer->pending_transactions.pop_front();

    peer->current_transaction->id = peer->trid_ctr++;
    
    if (peer->current_transaction->do_cb)
    {
        peer->current_transaction->do_cb(*this, peer, peer->current_transaction->id);
    }

    peer->transaction_timer_id = cv_reactor->get_timer().wait_ms(config.peer_transaction_timeout_ms,
        [w = weak_from_this(), peerw = peer_weak_ptr_t(peer)]() -> void
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_process_pending_transactions: Node expired!", t.get());
                return;
            }

            auto peer = peerw.lock();
            if (!peer)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_process_pending_transactions: Peer expired!", t.get());
                return;
            }

            if (!peer->current_transaction)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_process_pending_transactions: No current transaction for peer %04x!", t.get(), peer->node_id);
                return;
            }

            if (!peer->current_transaction->expiration_cb)
            {
                t->peer_complete_transaction(peer, peer->current_transaction->id, E_PEER_TRANSACTION_STATUS_PEER_EXPIRED);
                return;
            }

            peer->current_transaction->expiration_cb(*t, peer, peer->current_transaction->id);
        });
}

void node::peer_retry_transaction(const peer_ptr_t& peer)
{
    if (!peer->current_transaction)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_retry_transaction: No current transaction for peer %04x!", this, peer->node_id);
        return;
    }

    peer->current_transaction->do_cb(*this, peer, peer->current_transaction->id);
}

void node::peer_complete_transaction(const peer_ptr_t& peer, uint8_t id, int code)
{
    if (!peer->current_transaction)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_complete_transaction: No current transaction for peer %04x!", this, peer->node_id);
        return;
    }

    auto& transaction = *peer->current_transaction;
    if (transaction.id != id)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_complete_transaction: Unexpected transaction ID %u for peer %04x!", this, id, peer->node_id);
        return;
    }

    if (transaction.done_cb)
    {
        transaction.done_cb(*this, peer, id, code);
    }

    cv_reactor->get_timer().cancel(peer->transaction_timer_id);
    peer->current_transaction.reset();
    peer_process_pending_transactions(peer);
}

void node::peer_expire_security_context(const peer_ptr_t& peer, uint8_t id, uint64_t creation_id)
{
    security_ctx_s* sec_ctx = peer_find_sec_ctx(peer, id);
    if (!sec_ctx)
    {
        return;
    }

    if (creation_id != sec_ctx->creation_id)
    {
        return;
    }

    cv_reactor->get_timer().cancel(sec_ctx->timer_id);
    peer->security_contexts.erase(
        std::remove_if(peer->security_contexts.begin(), peer->security_contexts.end(),
            [&id, creation_id](const security_ctx_s& ctx)
            {
                return ctx.id == id && ctx.creation_id == creation_id;
            }),
        peer->security_contexts.end());
}

void node::peer_schedule_check_activity()
{
    check_peer_activity_timer_id = cv_reactor->get_timer().wait_ms(1000*15, [w = weak_from_this()]()
    {
        auto t = w.lock();
        if (!t)
        {
            return;
        }
        t->peer_schedule_check_activity();
    });

    std::vector<node_id_t> inactive_peers;
    for (auto& peer_ : peers)
    {
        auto& node_id = peer_.first;
        auto& peer = peer_.second;

        if (now_s() <= peer->last_activity_time_s + 15)
        {
            continue;
        }

        inactive_peers.push_back(node_id);
        peer_cleanup(peer);
    }

    for (auto& node_id : inactive_peers)
    {
        peers.erase(node_id);
    }
}

void node::peer_cleanup(const peer_ptr_t& peer)
{
    cv_reactor->get_timer().cancel(peer->transaction_timer_id);

    if (peer->current_transaction)
    {
        peer_complete_transaction(peer, peer->current_transaction->id, E_PEER_TRANSACTION_STATUS_PEER_TEARDOWN);
    }

    for (auto& sec_ctx : peer->security_contexts)
    {
        cv_reactor->get_timer().cancel(sec_ctx.timer_id);
    }

    peer->security_contexts.clear();

    if (!network_security_procedure_ctx.network_key_candidates.empty() ||
        !network_security_procedure_ctx.ongoing_acquisition_peer.expired())
    {
        network_security_procedure_ctx.network_key_candidates.remove_if(
            [peer_id = peer->node_id](const network_key_candidate_s& c)
            {
                return c.peer_id == peer_id;
            });

        if (network_security_procedure_ctx.ongoing_acquisition_peer.lock() == peer)
        {
            network_security_procedure_ctx.ongoing_acquisition_peer.reset();
        }
    }

    peer_finish_network_key_acquisition_procedure(peer, E_PEER_TRANSACTION_STATUS_PEER_TEARDOWN);
}

void node::reconfigure()
{
    if (config.network_key_seeder)
    {
        network_seed_keys();
    }
    network_schedule_key_refresh();
}

bool node::decode_beacon(const frame_const_t& frame) const
{
    if (frame.get_payload_size() == 0)
    {
        return true;
    }

    try
    {
        cum::per_codec_ctx ctx(
            const_cast<std::byte*>(frame.get_payload()),
            frame.get_payload_size());
        cum::beacon msg;
        cum::decode_per(msg, ctx);
        (void)msg;
        return true;
    }
    catch (const std::exception& e)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::decode_beacon: PER decode failed: %s", this, e.what());
        return false;
    }
}

bool node::network_has_keys() const
{
    const uint64_t now = now_s();
    for (const auto& ctx : network_security_contexts)
    {
        if (ctx.expiration_time_s > now)
        {
            return true;
        }
    }
    return false;
}

bool node::network_has_usable_key(uint8_t sec_ctx) const
{
    const uint64_t now = now_s();
    for (const auto& ctx : network_security_contexts)
    {
        if (ctx.sec_ctx == sec_ctx && ctx.expiration_time_s > now)
        {
            return true;
        }
    }
    return false;
}

void node::network_seed_keys()
{
    if (!network_security_contexts.empty())
    {
        return;
    }

    if (supported_integrity_algorithms.empty() || supported_confidentiality_algorithms.empty())
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::network_seed_keys: no supported algorithms configured!", this);
        return;
    }

    cum::network_key key;
    key.sec_ctx                   = 1;
    key.priority                  = random_u64();
    key.expiration_time_s         = now_s() + config.peer_security_ctx_timeout_s;
    key.integrity_algorithm       = supported_integrity_algorithms.front();
    key.confidentiality_algorithm = supported_confidentiality_algorithms.front();
    key.integrity_key             = to_cum_key(random_bytes(integrity_key_size(key.integrity_algorithm)));
    key.confidentiality_key       = to_cum_key(random_bytes(confidentiality_key_size(key.confidentiality_algorithm)));

    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::network_seed_keys: seeded network key sec_ctx=%u", this, key.sec_ctx);
    network_install_key(key);
}

uint64_t node::network_get_next_creation_id() const
{
    std::set<uint64_t> used;
    for (const auto& ctx : network_security_contexts)
    {
        used.insert(ctx.creation_id);
    }

    uint64_t id = 0;
    while (used.count(id))
    {
        ++id;
    }
    return id;
}

void node::network_install_key(const cum::network_key& key)
{
    if (key.expiration_time_s <= now_s())
    {
        return;
    }

    auto ctx_it = std::find_if(network_security_contexts.begin(), network_security_contexts.end(),
        [&key](const network_key_ctx_s& ctx) { return ctx.sec_ctx == key.sec_ctx; });

    if (network_security_contexts.end() != ctx_it &&
        !key_preference_is_better(key.priority, key.expiration_time_s,
                                  ctx_it->priority, ctx_it->expiration_time_s, now_s()))
    {
        return;
    }

    const uint64_t creation_id = network_get_next_creation_id();

    if (network_security_contexts.end() != ctx_it)
    {
        cv_reactor->get_timer().cancel(ctx_it->expiration_timer_id);
        network_security_contexts.erase(ctx_it);
    }

    network_key_ctx_s ctx;
    ctx.sec_ctx                   = key.sec_ctx;
    ctx.priority                  = key.priority;
    ctx.expiration_time_s         = key.expiration_time_s;
    ctx.creation_id               = creation_id;
    ctx.integrity_algorithm       = key.integrity_algorithm;
    ctx.confidentiality_algorithm = key.confidentiality_algorithm;
    ctx.integrity_key             = from_cum_key(key.integrity_key);
    ctx.confidentiality_key       = from_cum_key(key.confidentiality_key);

    const uint64_t ttl_s = std::max(key.expiration_time_s, now_s()) - now_s();
    ctx.expiration_timer_id = cv_reactor->get_timer().wait_ms(ttl_s * 1000,
        [w = weak_from_this(), sec_ctx = key.sec_ctx, creation_id]()
        {
            auto t = w.lock();
            if (!t)
            {
                return;
            }
            t->network_expire_key(sec_ctx, creation_id);
        });

    network_security_contexts.emplace_back(std::move(ctx));
}

void node::network_install_keys(const cum::network_keys& keys)
{
    for (const auto& key : keys)
    {
        network_install_key(key);
    }
}

cum::network_keys node::network_export_keys() const
{
    cum::network_keys keys;
    const uint64_t now = now_s();
    for (const auto& ctx : network_security_contexts)
    {
        if (ctx.expiration_time_s <= now)
        {
            continue;
        }

        cum::network_key key;
        key.sec_ctx                   = ctx.sec_ctx;
        key.priority                  = ctx.priority;
        key.expiration_time_s         = ctx.expiration_time_s;
        key.integrity_algorithm       = ctx.integrity_algorithm;
        key.confidentiality_algorithm = ctx.confidentiality_algorithm;
        key.integrity_key             = to_cum_key(ctx.integrity_key);
        key.confidentiality_key       = to_cum_key(ctx.confidentiality_key);
        keys.push_back(std::move(key));
    }
    return keys;
}

cum::network_key_informations node::network_export_key_informations() const
{
    cum::network_key_informations infos;
    const uint64_t now = now_s();
    for (const auto& ctx : network_security_contexts)
    {
        if (ctx.expiration_time_s <= now)
        {
            continue;
        }

        cum::network_key_information info;
        info.sec_ctx           = ctx.sec_ctx;
        info.priority          = ctx.priority;
        info.expiration_time_s = ctx.expiration_time_s;
        infos.push_back(info);
    }
    return infos;
}

void node::network_expire_key(uint8_t sec_ctx, uint64_t creation_id)
{
    network_security_contexts.erase(
        std::remove_if(network_security_contexts.begin(), network_security_contexts.end(),
            [sec_ctx, creation_id](const network_key_ctx_s& ctx)
            {
                return ctx.sec_ctx == sec_ctx && ctx.creation_id == creation_id;
            }),
        network_security_contexts.end());
}

void node::network_acquire_keys()
{
    if (network_security_procedure_ctx.key_information_collect_timer_id.has_value() ||
        network_key_acquisition_in_progress())
    {
        return;
    }

    network_start_security_query_procedure();
}

void node::network_start_security_query_procedure()
{
    if (network_security_procedure_ctx.key_information_collect_timer_id.has_value())
    {
        return;
    }

    auto& sec_proc = network_security_procedure_ctx;
    sec_proc.network_key_candidates.clear();
    sec_proc.ongoing_acquisition_peer.reset();

    cum::query_network_security msg;
    network_send_public_message(msg);

    sec_proc.key_information_collect_timer_id = cv_reactor->get_timer().wait_ms(
        config.network_security_query_timeout_ms,
        [w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t || !t->network_security_procedure_ctx.key_information_collect_timer_id)
            {
                return;
            }
            t->network_finish_security_query_procedure();
        });
}

void node::network_build_acquisition_candidates()
{
    auto& sec_proc = network_security_procedure_ctx;
    sec_proc.ongoing_acquisition_peer.reset();

    const uint64_t now = now_s();
    sec_proc.network_key_candidates.remove_if(
        [this, now](const network_key_candidate_s& entry)
        {
            if (peers.find(entry.peer_id) == peers.end())
            {
                return true;
            }
            return entry.expiration_time_s <= now;
        });
}

void node::network_finish_security_query_procedure()
{
    cv_reactor->get_timer().cancel(*network_security_procedure_ctx.key_information_collect_timer_id);
    network_security_procedure_ctx.key_information_collect_timer_id.reset();

    network_build_acquisition_candidates();

    if (network_security_procedure_ctx.network_key_candidates.empty())
    {
        network_security_procedure_ctx.ongoing_acquisition_peer.reset();
        log(*g_logger, E_LOG_BIT_INFO, "node[%p]::network_finish_security_query_procedure: no peer with network keys!", this);
        return;
    }

    network_start_next_key_acquisition();
}

void node::network_start_next_key_acquisition()
{
    auto& sec_proc = network_security_procedure_ctx;
    if (!sec_proc.ongoing_acquisition_peer.expired())
    {
        return;
    }

    while (!sec_proc.network_key_candidates.empty())
    {
        std::unordered_map<node_id_t, size_t> key_counts;
        for (const auto& candidate : sec_proc.network_key_candidates)
        {
            key_counts[candidate.peer_id]++;
        }

        node_id_t best_peer_id = 0;
        size_t best_count = 0;
        for (const auto& [peer_id, count] : key_counts)
        {
            if (count > best_count)
            {
                best_count = count;
                best_peer_id = peer_id;
            }
        }

        auto it = peers.find(best_peer_id);
        if (it == peers.end() || !it->second || it->second->net_key_acq_proc_ctx)
        {
            sec_proc.network_key_candidates.remove_if(
                [best_peer_id](const network_key_candidate_s& candidate)
                {
                    return candidate.peer_id == best_peer_id;
                });
            continue;
        }

        auto peer = it->second;
        sec_proc.ongoing_acquisition_peer = peer;
        peer_start_network_key_acquisition_procedure(peer,
            [peerw = peer_weak_ptr_t(peer)](node& t, const peer_ptr_t&, int code)
            {
                t.network_on_key_acquisition_complete(peerw.lock(), code);
            });
        return;
    }

    sec_proc.ongoing_acquisition_peer.reset();
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::network_start_next_key_acquisition: no remaining acquisition candidates!", this);
}

void node::network_on_key_acquisition_complete(const peer_ptr_t& peer, int code)
{
    auto& sec_proc = network_security_procedure_ctx;
    if (sec_proc.ongoing_acquisition_peer.lock() == peer)
    {
        sec_proc.ongoing_acquisition_peer.reset();
    }

    if (code != E_PEER_TRANSACTION_STATUS_OK && peer)
    {
        sec_proc.network_key_candidates.remove_if(
            [peer_id = peer->node_id](const network_key_candidate_s& candidate)
            {
                return candidate.peer_id == peer_id;
            });
    }

    if (code == E_PEER_TRANSACTION_STATUS_OK &&
        network_has_keys() &&
        sec_proc.network_key_candidates.empty())
    {
        sec_proc.ongoing_acquisition_peer.reset();
        network_schedule_key_refresh();
        return;
    }

    if (!sec_proc.network_key_candidates.empty())
    {
        network_start_next_key_acquisition();
        return;
    }

    sec_proc.ongoing_acquisition_peer.reset();
    log(*g_logger, E_LOG_BIT_INFO,
        "node[%p]::network_on_key_acquisition_complete: acquisition ended for peer %04x (status %s), no candidates left!",
        this, peer ? peer->node_id : 0, to_string(static_cast<peer_transaction_status_e>(code)));
}

void node::network_schedule_key_refresh()
{
    cv_reactor->get_timer().cancel(network_key_refresh_timer_id);
    if (!network_has_keys() || config.network_key_refresh_interval_s == 0)
    {
        return;
    }

    network_key_refresh_timer_id = cv_reactor->get_timer().wait_ms(
        config.network_key_refresh_interval_s * 1000,
        [w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t)
            {
                return;
            }
            t->network_send_key_refresh();
            t->network_schedule_key_refresh();
        });
}

void node::network_send_key_refresh()
{
    if (!selected_downstream_identity || !network_has_keys())
    {
        return;
    }

    const auto keys = network_export_keys();
    if (keys.empty())
    {
        return;
    }

    const auto* ctx = network_get_key_ctx(keys.front().sec_ctx);
    if (!ctx)
    {
        return;
    }

    cum::network_key_refresh msg;
    msg.keys = keys;

    for (auto& [node_id, peer] : peers)
    {
        (void)node_id;
        if (!peer->preferred_peer_address.port)
        {
            continue;
        }

        bfc::sized_buffer pdu(1024 * 65);
        auto frame = prepare_frame(pdu);
        frame.set_ttl(1);
        frame.set_frame_type(frame_type_e::E_FRAME_TYPE_NETWORK);
        frame.set_sec_ctx(ctx->sec_ctx);
        frame.set_mac_size_units(0);
        frame.set_src(selected_downstream_identity->node_id);
        frame.set_dst(peer->node_id);
        frame.set_ts(now_s());
        frame.set_sn(peer->tx_sn++);

        if (!encode_payload(frame, msg))
        {
            continue;
        }

        pdu.resize(frame.get_size());
        network_try_send_frame(peer->preferred_peer_address.port, peer->preferred_peer_address.address, std::move(pdu), ctx->sec_ctx);
    }
}

bool node::network_key_acquisition_in_progress() const
{
    if (!network_security_procedure_ctx.ongoing_acquisition_peer.expired() ||
        (!network_security_procedure_ctx.key_information_collect_timer_id &&
         !network_security_procedure_ctx.network_key_candidates.empty()))
    {
        return true;
    }

    for (const auto& [node_id, peer] : peers)
    {
        (void)node_id;
        if (peer->net_key_acq_proc_ctx)
        {
            return true;
        }
    }
    return false;
}

void node::peer_start_network_key_acquisition_procedure(const peer_ptr_t& peer, procedure_completion_cb_t completion)
{
    if (!peer || peer->net_key_acq_proc_ctx)
    {
        return;
    }

    if (!peer_select_sec_ctx(peer))
    {
        peer_start_security_procedure(peer,
            [completion = std::move(completion)](node& t, const peer_ptr_t& acquisition_peer, int code) mutable
            {
                if (code != E_PEER_TRANSACTION_STATUS_OK)
                {
                    if (completion)
                    {
                        completion(t, acquisition_peer, code);
                    }
                    return;
                }

                t.peer_start_network_key_acquisition_procedure(acquisition_peer, std::move(completion));
            },
            false);
        return;
    }

    if (!selected_downstream_identity)
    {
        if (completion)
        {
            completion(*this, peer, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
        }
        return;
    }

    const uint8_t request_id = peer->trid_ctr++;
    peer->net_key_acq_proc_ctx.emplace(
        network_key_acquisition_procedure_ctx_s{request_id, 0, 0, {}, {}, std::move(completion)});

    transaction_cb_t do_cb = [request_id](node& t, const peer_ptr_t& peer, uint8_t id) -> void
    {
        if (!peer->net_key_acq_proc_ctx || peer->net_key_acq_proc_ctx->request_id != request_id)
        {
            t.peer_complete_transaction(peer, id, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
            return;
        }

        cum::network_keys_request msg;
        msg.id = request_id;

        if (!t.peer_send_message(peer, peer->preferred_peer_address.port, peer->preferred_peer_address.address, msg))
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_network_key_acquisition_procedure: Failed to send request to peer %04x!", &t, peer->node_id);
            t.peer_complete_transaction(peer, id, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
        }
    };

    expiration_cb_t expiration_cb = [request_id](node& t, const peer_ptr_t& peer, uint8_t id) -> void
    {
        if (!peer->net_key_acq_proc_ctx || peer->net_key_acq_proc_ctx->request_id != request_id)
        {
            return;
        }

        if (!peer->current_transaction || peer->current_transaction->id != id)
        {
            return;
        }

        if (peer->net_key_acq_proc_ctx->retry_count >= 3)
        {
            t.peer_complete_transaction(peer, id, E_PEER_TRANSACTION_STATUS_MAX_RETRIES_REACHED);
            return;
        }

        peer->net_key_acq_proc_ctx->retry_count++;
        t.peer_retry_transaction(peer);
    };

    completion_cb_t done_cb = [](node& t, const peer_ptr_t& peer, uint8_t id, int code) -> void
    {
        log(*g_logger, E_LOG_BIT_INFO, "node[%p]::completion_cb_t: Network key acquisition transaction %u completed with status %s for peer %04x!", &t, id, to_string(static_cast<peer_transaction_status_e>(code)), peer->node_id);

        t.peer_finish_network_key_acquisition_procedure(peer, code);
    };

    peer->pending_transactions.emplace_back(peer_transaction_s{peer->trid_ctr++, std::move(do_cb), std::move(done_cb), std::move(expiration_cb)});
    peer_process_pending_transactions(peer);
}

void node::network_send_keys_response(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& to, const cum::network_keys_response& msg)
{
    if (!peer_send_message(peer, port, to, msg))
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::network_send_keys_response: Failed to send response to peer %04x!", this, peer->node_id);
    }
}

void node::network_send_security_information(const port_ptr_t& port, const sockaddr_t& to)
{
    if (!selected_downstream_identity || !port)
    {
        return;
    }

    const auto all = network_export_key_informations();
    const size_t mtu = port->mtu == 0 ? 1500 : port->mtu;
    const auto ts = static_cast<uint32_t>(now_s());
    const node_id_t src = selected_downstream_identity->node_id;

    auto encode_chunk = [&](const cum::network_security_information& msg) -> std::optional<bfc::sized_buffer>
    {
        bfc::sized_buffer pdu(mtu);
        auto frame = prepare_frame(pdu);
        frame.set_ttl(0);
        frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PUBLIC);
        frame.set_sec_ctx(k_sec_ctx_none);
        frame.set_mac_size_units(0);
        frame.set_sn(0);
        frame.set_src(src);
        frame.set_dst(0xFFFFFFFF);
        frame.set_ts(ts);

        if (!encode_payload(frame, msg))
        {
            return std::nullopt;
        }

        pdu.resize(frame.get_size());
        return pdu;
    };

    auto send_chunk = [&](const cum::network_security_information& msg) -> bool
    {
        auto pdu = encode_chunk(msg);
        if (!pdu)
        {
            return false;
        }
        port->transport->out.push(transport_out_t{transport_data_s{0, to, std::move(*pdu)}});
        return true;
    };

    if (all.empty())
    {
        if (!send_chunk(cum::network_security_information{}))
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::network_send_security_information: Failed to encode empty response!", this);
        }
        return;
    }

    size_t offset = 0;
    while (offset < all.size())
    {
        cum::network_security_information msg;
        std::optional<bfc::sized_buffer> encoded;
        while (offset < all.size())
        {
            msg.informations.push_back(all[offset]);
            auto candidate = encode_chunk(msg);
            if (!candidate)
            {
                msg.informations.pop_back();
                break;
            }
            encoded = std::move(candidate);
            ++offset;
        }

        if (!encoded)
        {
            log(*g_logger, E_LOG_BIT_ERROR,
                "node[%p]::network_send_security_information: single network_key_information exceeds mtu %zu!",
                this, mtu);
            return;
        }

        port->transport->out.push(transport_out_t{transport_data_s{0, to, std::move(*encoded)}});
    }
}

template<typename Msg>
void node::network_send_public_message(const Msg& msg)
{
    if (!selected_downstream_identity)
    {
        return;
    }

    for (const auto& beacon : beacons)
    {
        if (!beacon || !beacon->peer.port)
        {
            continue;
        }

        bfc::sized_buffer pdu(1024 * 65);
        auto frame = prepare_frame(pdu);
        frame.set_ttl(0);
        frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PUBLIC);
        frame.set_sec_ctx(k_sec_ctx_none);
        frame.set_mac_size_units(0);
        frame.set_sn(0);
        frame.set_src(selected_downstream_identity->node_id);
        frame.set_dst(0xFFFFFFFF);
        frame.set_ts(now_s());

        if (!encode_payload(frame, msg))
        {
            continue;
        }

        pdu.resize(frame.get_size());
        beacon->peer.port->transport->out.push(transport_out_t{transport_data_s{0, beacon->peer.address, std::move(pdu)}});
    }

    for (auto& [node_id, peer] : peers)
    {
        (void)node_id;
        if (!peer->preferred_peer_address.port)
        {
            continue;
        }

        bfc::sized_buffer pdu(1024 * 65);
        auto frame = prepare_frame(pdu);
        frame.set_ttl(0);
        frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PUBLIC);
        frame.set_sec_ctx(k_sec_ctx_none);
        frame.set_mac_size_units(0);
        frame.set_sn(0);
        frame.set_src(selected_downstream_identity->node_id);
        frame.set_dst(peer->node_id);
        frame.set_ts(now_s());

        if (!encode_payload(frame, msg))
        {
            continue;
        }

        pdu.resize(frame.get_size());
        peer->preferred_peer_address.port->transport->out.push(
            transport_out_t{transport_data_s{0, peer->preferred_peer_address.address, std::move(pdu)}});
    }
}

const network_key_ctx_s* node::network_get_key_ctx(uint8_t sec_ctx) const
{
    const uint64_t now = now_s();
    const network_key_ctx_s* fallback = nullptr;

    for (const auto& ctx : network_security_contexts)
    {
        if (ctx.sec_ctx != sec_ctx || ctx.expiration_time_s <= now)
        {
            continue;
        }

        if (ctx.expiration_time_s > now + k_sec_ctx_expiring_grace_s)
        {
            return &ctx;
        }

        if (!fallback)
        {
            fallback = &ctx;
        }
    }

    return fallback;
}

bool node::network_accept_rx(const frame_const_t& frame, bfc::const_buffer_view pdu)
{
    const uint8_t sec_ctx = frame.get_sec_ctx();

    if (!network_has_usable_key(sec_ctx))
    {
        log(*g_logger, E_LOG_BIT_INFO, "node[%p]::network_accept_rx: unknown network sec_ctx %u, dropping frame!", this, sec_ctx);
        network_acquire_keys();
        return false;
    }

    const auto* ctx = network_get_key_ctx(sec_ctx);
    if (!ctx || !verify_frame_mac(frame, pdu, ctx->integrity_algorithm, ctx->integrity_key))
    {
        log(*g_logger, E_LOG_BIT_INFO, "node[%p]::network_accept_rx: network MAC verification failed for sec_ctx %u, dropping frame!", this, sec_ctx);
        network_acquire_keys();
        return false;
    }

    return true;
}

bool node::network_try_send_frame(const port_ptr_t& port, const sockaddr_t& to, bfc::sized_buffer pdu, uint8_t sec_ctx)
{
    if (!network_has_usable_key(sec_ctx))
    {
        log(*g_logger, E_LOG_BIT_INFO, "node[%p]::network_try_send_frame: no network key for sec_ctx %u, dropping frame!", this, sec_ctx);
        network_acquire_keys();
        return false;
    }

    port->transport->out.push(transport_out_t{transport_data_s{0, to, std::move(pdu)}});
    return true;
}

uint64_t node::now_s() const
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

template<typename Msg>
bool node::peer_send_message(const peer_ptr_t& peer, const port_ptr_t& port, const sockaddr_t& to, const Msg& msg)
{
    if (!selected_downstream_identity)
    {
        return false;
    }

    bfc::sized_buffer pdu(1024 * 65);
    auto frame = prepare_frame(pdu);
    frame.set_ttl(1);
    frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PEER);
    frame.set_sec_ctx(k_sec_ctx_none);
    frame.set_mac_size_units(0);
    frame.set_src(selected_downstream_identity->node_id);
    frame.set_dst(peer->node_id);
    frame.set_ts(now_s());
    frame.set_sn(peer->tx_sn++);

    if (!encode_payload(frame, msg))
    {
        return false;
    }

    pdu.resize(frame.get_size());
    port->transport->out.push(transport_out_t{transport_data_s{0, to, std::move(pdu)}});
    return true;
}

template bool node::peer_send_message<cum::network_keys_request>(const peer_ptr_t&, const port_ptr_t&, const sockaddr_t&, const cum::network_keys_request&);
template bool node::peer_send_message<cum::network_keys_response>(const peer_ptr_t&, const port_ptr_t&, const sockaddr_t&, const cum::network_keys_response&);
template void node::network_send_public_message<cum::query_network_security>(const cum::query_network_security&);

} // namespace bfc_tunnel
