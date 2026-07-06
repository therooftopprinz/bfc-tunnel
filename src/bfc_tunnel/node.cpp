#include "bfc/sized_buffer.hpp"
#include <bfc_tunnel/transport/transport_types.hpp>
#include <bfc_tunnel/node.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/utils/reactor_helper.hpp>
#include <bfc_tunnel/utils/logger.hpp>
#include <bfc_tunnel/utils/msg_utils.hpp>
#include <bfc_tunnel/utils/number_utils.hpp>
#include <bfc_tunnel/security/key_utils.hpp>

#include <chrono>
#include <cstring>

namespace bfc_tunnel
{

const char* to_string(peer_transaction_status_e status)
{
    switch (status)
    {
        case E_PEER_TRANSACTION_STATUS_OK: return "OK";
        case E_PEER_TRANSACTION_STATUS_MAX_RETRIES_REACHED: return "MAX_RETRIES_REACHED";
        case E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS: return "NOT_IN_PROGRESS";
        case E_PEER_TRANSACTION_STATUS_VERIFICATION_FAILED: return "VERIFICATION_FAILED";
        case E_PEER_TRANSACTION_STATUS_UNSUPPORTED: return "UNSUPPORTED";
        case E_PEER_TRANSACTION_STATUS_NO_SUPPORTED_ALGORITHM_PAIR: return "NO_SUPPORTED_ALGORITHM_PAIR";
        case E_PEER_TRANSACTION_STATUS_PEER_TEARDOWN: return "PEER_TEARDOWN";
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
    
    pdu.resize(frame.get_header_size());
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
            handle_btp_message(port, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_MSG2:
        {
            cum::msg2 msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_EXCHANGE_NETWORK_KEYS:
        {
            cum::exchange_network_keys msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_LINK_INFO:
        {
            cum::link_info msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_LINK_REPORT:
        {
            cum::link_report msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_ROUTE_ANNOUNCE:
        {
            cum::route_announce msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, frame, msg);
            break;
        }
        case E_PAYLOAD_TYPE_N2N_INDICATION:
        {
            cum::n2n_indication msg;
            cum::decode_per(msg, ctx);
            handle_btp_message(port, frame, msg);
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

void node:: handle_beacon(const port_ptr_t& port, const sockaddr_t& from, const frame_const_t& frame)
{
    const node_id_t peer_node_id = frame.get_src();
    auto peer_it = peers.find(peer_node_id);
    if (peers.end() == peer_it)
    {
        auto public_key_it = public_keys.find(peer_node_id);
        if (public_keys.end() == public_key_it)
        {
            return;
        }

        auto peer = peer_create(peer_node_id, public_key_it->second);
        peer->preferred_peer_address = peer_address_s{port, from};
        peer_update_link_activity(peer, port, from, frame.get_size());

        if (port->type == port_s::UNICAST)
        {
            peer_start_security_procedure(peer);
        }

        return;
    }

    auto peer = peer_it->second;
    peer_update_link_activity(peer, port, from, frame.get_size());

    if (port->type == port_s::UNICAST)
    {
        peer_start_security_procedure(peer);
    }
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const frame_const_t& /*frame*/, cum::msg1& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Msg1!", this);
}

void node::handle_btp_message(const port_ptr_t& port, const frame_const_t& frame, cum::msg2& msg)
{
    const node_id_t peer_node_id = frame.get_src();
    auto peer_it = peers.find(peer_node_id);
    if (peers.end() == peer_it)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(msg2): unexpected peer %04x!", this, peer_node_id);
        return;
    }

    auto peer = peer_it->second;
    peer_update_link_activity(peer, port, port->interface_address, frame.get_size());

    if (!peer->sec_proc_ctx)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_btp_message(msg2): no security procedure in progress for peer %04x!", this, peer_node_id);
        peer_complete_transaction(peer, msg.id, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
        return;
    }

    auto& sec_proc_ctx = peer->sec_proc_ctx.value();

    if (msg.status == cum::status_e::UNSUPPORTED)
    {
        peer->sec_proc_ctx->state = security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_UNSUPPORTED;
        peer_retry_transaction(peer);
        return;
    }

    if (msg.status == cum::status_e::VERIFICATION_FAILED)
    {
        peer_complete_transaction(peer, msg.id, E_PEER_TRANSACTION_STATUS_VERIFICATION_FAILED);
        return;
    }

    if (msg.status == cum::status_e::OK)
    {
        peer->sec_proc_ctx->dhke.set_peer_ephemeral_public(msg.ephemeral);

        auto& sec_ctx = peer_get_or_create_sec_ctx(peer, sec_proc_ctx.id);
        sec_ctx.confidentiality_algorithm  = sec_proc_ctx.msg1.confidentiality_algorithm;
        sec_ctx.integrity_algorithm        = sec_proc_ctx.msg1.integrity_algorithm;
        sec_ctx.integrity_key              = sec_proc_ctx.dhke.derive_integrity_key(sec_ctx.integrity_algorithm);
        sec_ctx.confidentiality_key        = sec_proc_ctx.dhke.derive_confidentiality_key(sec_ctx.confidentiality_algorithm);
    }

    peer_complete_transaction(peer, msg.id, E_PEER_TRANSACTION_STATUS_OK);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const frame_const_t& /*frame*/, cum::exchange_network_keys& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Exchange network keys!", this);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const frame_const_t& /*frame*/, cum::link_info& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Link info!", this);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const frame_const_t& /*frame*/, cum::link_report& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Link report!", this);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const frame_const_t& /*frame*/, cum::route_announce& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Route announce!", this);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const frame_const_t& /*frame*/, cum::n2n_indication& /*msg*/)
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
        links.emplace_back(std::make_pair(peer->preferred_peer_address, peer_link_ctx_s{}));
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

peer_ptr_t node::peer_create(const node_id_t& node_id, const peer_public_key_s& public_key)
{
    auto peer = std::make_shared<peer_s>();
    peer->node_id = node_id;
    peer->public_key = public_key;
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

security_ctx_s& node::peer_get_or_create_sec_ctx(const peer_ptr_t& peer, uint8_t id)
{
    auto ctx_it = std::find_if(peer->security_contexts.begin(), peer->security_contexts.end(), [&id](const security_ctx_s& ctx) { return ctx.id == id; });
    if (peer->security_contexts.end() == ctx_it)
    {
        peer->security_contexts.emplace_back(security_ctx_s{id, {}, {}, 0, 0, {}, false});
        return peer->security_contexts.back();
    }

    return *ctx_it;
}

void node::peer_start_security_procedure(const peer_ptr_t& peer, bool force)
{
    if (!peer->security_contexts.empty() && !force)
    {
        return;
    }

    if (!peer->sec_proc_ctx)
    {
        uint8_t ctx_id = peer_get_next_sec_ctx_id(peer);

        dhke_kk dhke(make_dh_backend(peer->public_key.key_type));
        if (selected_downstream_identity)
        {
            dhke.setup_handshake(selected_downstream_identity->private_key, peer->public_key.public_key, true);
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
                std::move(dhke)});
    }

    if (peer->sec_proc_ctx->state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_ONGOING ||
        peer->sec_proc_ctx->state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_EXPIRED ||
        peer->sec_proc_ctx->state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_UNSUPPORTED)
    {   
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_security_procedure: Security procedure already in progress for peer %04x!", this, peer->node_id);
        return;
    }

    transaction_cb_t do_cb = [w = weak_from_this(), peerw = peer_weak_ptr_t(peer)](uint8_t id) -> void
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

        if (!peer->sec_proc_ctx)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_security_procedure: Security procedure not in progress for peer %04x!", t.get(), peer->node_id);
            t->peer_complete_transaction(peer, id, E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
            return;
        }

        auto& sec_proc_ctx = peer->sec_proc_ctx.value();

        if (sec_proc_ctx.state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_UNSUPPORTED)
        {
            peer->supported_algorithm_pairs.erase(sec_proc_ctx.algo_pair_used);
            peer->test_algorithm_pairs.erase(sec_proc_ctx.algo_pair_used);
            t->peer_complete_transaction(peer, id, E_PEER_TRANSACTION_STATUS_UNSUPPORTED);
            t->peer_start_security_procedure(peer);
            return;
        }

        if (sec_proc_ctx.state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_QUEUED)
        {
            sec_proc_ctx.pdu = bfc::sized_buffer(1024*65);
            sec_proc_ctx.frame = prepare_frame(sec_proc_ctx.pdu);
            sec_proc_ctx.frame.set_ttl(0);
            sec_proc_ctx.frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PEER);
            sec_proc_ctx.frame.set_sec_ctx(k_sec_ctx_none);
            sec_proc_ctx.frame.set_mac_size_units(0);
            sec_proc_ctx.frame.set_src(t->selected_downstream_identity->node_id);
            sec_proc_ctx.frame.set_dst(peer->node_id);

            sec_proc_ctx.msg1.id          = id;
            sec_proc_ctx.msg1.sec_ctx     = sec_proc_ctx.id;
            sec_proc_ctx.msg1.dh_key_type = static_cast<uint8_t>(peer->public_key.key_type);
            sec_proc_ctx.dhke.get_own_ephemeral_public(sec_proc_ctx.msg1.ephemeral);
            sec_proc_ctx.msg1.signature   = {};
    
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
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_security_procedure: No supported or test algorithm pairs for peer %04x!", t.get(), peer->node_id);
                t->peer_complete_transaction(peer, id, E_PEER_TRANSACTION_STATUS_NO_SUPPORTED_ALGORITHM_PAIR);
                return;
            }
    
            sec_proc_ctx.msg1.integrity_algorithm = integrity_algorithm;
            sec_proc_ctx.msg1.confidentiality_algorithm = confidentiality_algorithm;
    
            std::byte msg1_buf[64];
            auto msg1_buf_view = bfc::buffer_view(msg1_buf, sizeof(msg1_buf));
            cum::per_codec_ctx ctx(msg1_buf, sizeof(msg1_buf));
            cum::encode_per(sec_proc_ctx.msg1, ctx);
            msg1_buf_view = bfc::buffer_view(msg1_buf, sizeof(msg1_buf)-ctx.size());
            sec_proc_ctx.msg1.signature = sign(static_cast<dh_key_type_e>(peer->public_key.key_type), t->selected_downstream_identity->private_key, msg1_buf_view);
    
            if (!encode_payload(sec_proc_ctx.frame, sec_proc_ctx.msg1))
            {
                log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_security_procedure: Failed to encode MSG1 for peer %u!", t.get(), peer->node_id);
                peer->sec_proc_ctx.reset();
                return;
            }
            
            sec_proc_ctx.pdu.resize(sec_proc_ctx.frame.get_size());
        }

        sec_proc_ctx.frame.set_ts(t->now_s());
        sec_proc_ctx.frame.set_sn(peer->tx_sn++);

        auto pdu = bfc::sized_buffer(sec_proc_ctx.pdu.size());
        std::memcpy(pdu.data(), sec_proc_ctx.pdu.data(), pdu.size());
        peer->preferred_peer_address.port->transport->out.push(transport_out_t{transport_data_s{0, peer->preferred_peer_address.address, std::move(pdu)}});

        sec_proc_ctx.state = security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_ONGOING;
        sec_proc_ctx.retry_count++;
    };

    expiration_cb_t expiration_cb = [w = weak_from_this(), peerw = peer_weak_ptr_t(peer)](uint8_t id) -> void
    {
        auto t = w.lock();
        if (!t)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[0]::expiration_cb_t: Node expired!");
            return;
        }

        auto peer = peerw.lock();
        if (!peer)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::expiration_cb_t: Peer expired!", t.get());
            return;
        }

        if (!peer->current_transaction)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::expiration_cb_t: No current transaction for peer %04x!", t.get(), peer->node_id);
            return;
        }

        if (id != peer->sec_proc_ctx->msg1.id || id != peer->current_transaction->id)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::expiration_cb_t: Unexpected transaction ID %u for peer %04x!", t.get(), id, peer->node_id);
            return;
        }

        if (!peer->sec_proc_ctx)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::expiration_cb_t: Security procedure not in progress for peer %04x!", t.get(), peer->node_id);
            t->peer_complete_transaction(peer, 0,E_PEER_TRANSACTION_STATUS_NOT_IN_PROGRESS);
            return;
        }
        
        auto& sec_proc_ctx = peer->sec_proc_ctx;

        if (sec_proc_ctx->state == security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_ONGOING && sec_proc_ctx->retry_count >= 3)
        {
            t->peer_complete_transaction(peer, 0, E_PEER_TRANSACTION_STATUS_MAX_RETRIES_REACHED);
            return;
        }

        sec_proc_ctx->state = security_procedure_ctx_s::E_SEC_PROC_STATE_SENDER_EXPIRED;
        t->peer_retry_transaction(peer);
    };

    completion_cb_t done_cb = [w = weak_from_this(), peerw = peer_weak_ptr_t(peer)](uint8_t id, int code) -> void
    {
        auto t = w.lock();
        if (!t)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[0]::completion_cb_t: Node expired!");
            return;
        }

        auto peer = peerw.lock();
        if (!peer)
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::completion_cb_t: Peer expired!", t.get());
            return;
        }

        log(*g_logger, E_LOG_BIT_INFO, "node[%p]::completion_cb_t: Transaction %u completed with status %s for peer %04x!", t.get(), id, to_string(static_cast<peer_transaction_status_e>(code)), peer->node_id);

        peer->sec_proc_ctx.reset();
    };

    peer->pending_transactions.emplace_back(peer_transaction_s{peer->trid_ctr++, std::move(do_cb), nullptr, std::move(expiration_cb)});
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
        peer->current_transaction->do_cb(peer->current_transaction->id);
    }
}

void node::peer_retry_transaction(const peer_ptr_t& peer)
{
    if (!peer->current_transaction)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_retry_transaction: No current transaction for peer %04x!", this, peer->node_id);
        return;
    }

    peer->current_transaction->do_cb(peer->current_transaction->id);
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
        transaction.done_cb(id, code);
    }

    peer->current_transaction.reset();
    peer_process_pending_transactions(peer);
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
    peer_complete_transaction(peer, 0, E_PEER_TRANSACTION_STATUS_PEER_TEARDOWN);
}

uint64_t node::now_s()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace bfc_tunnel
