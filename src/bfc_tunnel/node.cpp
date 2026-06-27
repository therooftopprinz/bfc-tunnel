#include "bfc/sized_buffer.hpp"
#include "bfc_tunnel/transport/transport_types.hpp"
#include <bfc_tunnel/node.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/utils/reactor_helper.hpp>
#include <bfc_tunnel/utils/logger.hpp>

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

                t->state = E_NODE_STATE_INITIALIZED;
            }
        );
}

void node::uninitialize()
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t || E_NODE_STATE_UNINITIALIZED == t->state)
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
            t->state = E_NODE_STATE_UNINITIALIZED;
        });
}

void node::add_port(port_ptr_t port)
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

            if (E_NODE_STATE_INITIALIZED != t->state)
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

void node::rem_port(port_ptr_t port)
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

void node::on_beacon_timer_expired(beacon_ptr_t beacon)
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

void node::send_beacon(beacon_ptr_t beacon)
{
    bfc::sized_buffer pdu(1024*65);

    uint64_t ts = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    frame_t frame(reinterpret_cast<uint8_t*>(pdu.data()), pdu.size());
    frame.set_ttl(0);
    frame.set_reserved(0);
    frame.set_version(1);
    frame.set_frame_type(frame_type_e::E_FRAME_TYPE_PUBLIC);
    frame.set_sec_ctx(0);
    frame.set_int_algo(E_EA_NONE);
    frame.set_conf_algo(E_CA_NONE);
    frame.set_mac_size(0);
    frame.set_sn(0);
    frame.set_ts(ts);
    frame.set_src(node_id);
    frame.set_dst(0xFFFFFFFF);
    frame.set_payload_type(payload_type_e::E_PAYLOAD_TYPE_BEACON);
    
    pdu.resize(frame.get_header_size());
    beacon->peer.port->transport->out.push(transport_out_t{transport_data_s{0, beacon->peer.address, std::move(pdu)}});
}

void node::handle_pdu(const port_ptr_t& port, const sock_buff_t& pdu)
{
    if (pdu.empty())
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_pdu: Empty PDU!", this);
        return;
    }

    auto pdu_raw = reinterpret_cast<const uint8_t*>(pdu.data());

    frame_const_t probe(pdu_raw, pdu.size());
    auto mac_size = integrity_mac_size(probe.get_int_algo());

    frame_const_t frame(pdu_raw, pdu.size(), mac_size);

    if (!validate_frame(frame))
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_pdu: Invalid frame!", this);
        return;
    }

    if (frame.get_payload_type() == E_PAYLOAD_TYPE_BEACON)
    {
        handle_beacon(port, frame);
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

void node::on_port_in_queue_ready(port_ptr_t port)
{
    auto pdu = port->transport->in.pop();
    if (pdu.empty())
    {
        return;
    }

    // handle_pdu(port, pdu);
}

void node::handle_beacon(const port_ptr_t& /*port*/, const frame_const_t& /*frame*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_beacon", this);
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

} // namespace bfc_tunnel
