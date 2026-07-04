#include "bfc/sized_buffer.hpp"
#include <bfc_tunnel/transport/transport_types.hpp>
#include <bfc_tunnel/node.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/utils/reactor_helper.hpp>
#include <bfc_tunnel/utils/logger.hpp>
#include <bfc_tunnel/utils/msg_utils.hpp>

#include <chrono>
#include <cstring>

namespace bfc_tunnel
{

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

    auto frame = prepare_frame(pdu, 0xFF, 0);

    frame.set_ttl(0);
    frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PUBLIC);
    frame.set_int_algo(E_EA_NONE);
    frame.set_conf_algo(E_CA_NONE);
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
        auto public_keys_it = public_keys.find(peer_node_id);
        if (public_keys.end() == public_keys_it)
        {
            return;
        }

        auto peer = std::make_shared<peer_s>();
        peer->node_id = peer_node_id;
        peer->public_keys = public_keys_it->second;
        peer->preffered_peer_address = peer_address_s{port, from};
        peer_update_link_activity(peer, port, from, frame.get_size());
        peers.emplace(peer_node_id, peer);
        peer_start_security_procedure(peer);
    }

    auto peer = peer_it->second;
    peer_update_link_activity(peer, port, from, frame.get_size());
    peer_start_security_procedure(peer);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const frame_const_t& /*frame*/, cum::msg1& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Msg1!", this);
}

void node::handle_btp_message(const port_ptr_t& /*port*/, const frame_const_t& /*frame*/, cum::msg2& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Msg2!", this);
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
    auto link_it = std::find_if(peer->links.begin(), peer->links.end(), [&from](const std::pair<peer_address_s, peer_link_ctx_s>& link) { return is_equal(link.first.address, from); });
    if (peer->links.end() == link_it)
    {
        peer->links.emplace_back(std::make_pair(peer->preffered_peer_address, peer_link_ctx_s{}));
        auto& link = peer->links.back();
        link.second.last_activity_time_us = std::chrono::steady_clock::now().time_since_epoch().count();
        link.second.recv_pkt++;
        link.second.recv_byt += size;
    }
    else
    {
        link_it->second.last_activity_time_us = std::chrono::steady_clock::now().time_since_epoch().count();
        link_it->second.recv_pkt++;
        link_it->second.recv_byt += size;
    }
}

void node::peer_start_security_procedure(const peer_ptr_t& peer, bool force)
{
    if (peer->sec_ctxs.empty() && !force)
    {
        return;
    }

    if (peer->sec_proc_ctx)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::peer_start_security_procedure: Security procedure already in progress for peer %u!", this, peer->node_id);
        return;
    }

    

    peer->sec_proc_ctx = sec_proc_ctx_s{dhke_kk(make_dh_backend(E_DHKT_X25519))};

    auto pdu = bfc::sized_buffer(1024*65);
    auto frame = prepare_frame(pdu, 0xFF, 0);
    frame.set_ttl(0);
    frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PEER);
    frame.set_int_algo(E_EA_NONE);
    frame.set_conf_algo(E_CA_NONE);
    frame.set_sn(peer->next_ctr++);
    frame.set_src(selected_downstream_identity->node_id);
    frame.set_dst(peer->node_id);
    frame.set_payload_type(payload_type_e::E_PAYLOAD_TYPE_MSG1);

    cum::msg1 msg1;
    

    pdu.resize(frame.get_header_size());
    peer->preffered_peer_address.port->transport->out.push(transport_out_t{transport_data_s{0, peer->preffered_peer_address.address, std::move(pdu)}});
}

} // namespace bfc_tunnel
