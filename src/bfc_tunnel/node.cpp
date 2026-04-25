#include <bfc_tunnel/node.hpp>
#include <bfc_tunnel/logger.hpp>
#include <bfc_tunnel/protocol.hpp>

namespace bfc_tunnel
{

node::node(
    cv_reactor_ptr_t cv_reactor,
    node_transport_queue_ptr_t transport_out,
    transport_node_queue_ptr_t transport_in,
    node_service_queue_ptr_t service_out,
    service_node_queue_ptr_t service_in)
    : cv_reactor(cv_reactor)
    , transport_out(transport_out)
    , transport_in(transport_in)
    , service_out(service_out)
    , service_in(service_in)
{}

node::~node()
{
    uninitialize();
}

void node::initialize()
{
    cv_reactor->wake_up(
            [w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::initialize: Weak pointer expired!");
                    return;
                }

                t->cv_reactor->add_read_rdy(*t->transport_in,
                    [w]()
                    {
                        auto t = w.lock();
                        if (!t)
                        {
                            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::on_transport_in_queue_ready: Weak pointer expired!");
                            return;
                        }
                        t->on_transport_in_queue_ready();
                    });
                t->cv_reactor->add_read_rdy(*t->service_in,
                    [w]()
                    {
                        auto t = w.lock();
                        if (!t)
                        {
                            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::on_service_in_queue_ready: Weak pointer expired!");
                            return;
                        }
                        t->on_service_in_queue_ready();
                    });
                t->state = E_NODE_STATE_INITIALIZED;
            }
        );
}

void node::on_transport_in_queue_ready()
{
    auto ins = transport_in->pop();
    if (ins.empty())
    {
        return;
    }

    for (auto& in : ins)
    {
        auto& header = *(header_s*) in.pdu.data();
        if (!validate_header(in.pdu))
        {
            log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::on_transport_in_queue_ready: Invalid header!", this);
            continue;
        }

        handle_transport_message(header, payload_view(&header));
    }
}

void node::on_service_in_queue_ready()
{
    auto sdus = service_in->pop();
    if (sdus.empty())
    {
        return;
    }
}

void node::handle_transport_message(const header_s& header, const bfc::buffer_view& payload)
{
    switch (header.type)
    {
        case E_MSG_TYPE_ID_REQUEST:
            handle_transport_message(header, *(id_request_s*) payload.data());
            break;
        case E_MSG_TYPE_ID_RESPONSE:
            handle_transport_message(header, *(id_response_s*) payload.data());
            break;
        case E_MSG_TYPE_LINK_INFO:
            handle_transport_message(header, *(link_info_s*) payload.data());
            break;
        case E_MSG_TYPE_LINK_REPORT:
            handle_transport_message(header, *(link_report_s*) payload.data());
            break;
        case E_MSG_TYPE_ROUTE_ANNOUNCE:
            handle_transport_message(header, *(route_announce_s*) payload.data());
            break;
        case E_MSG_TYPE_HUB_ANNOUNCE:
            handle_transport_message(header, *(hub_announce_s*) payload.data());
            break;
        case E_MSG_TYPE_N2N_INDICATION:
            handle_transport_message(header, *(n2n_indication_s*) payload.data());
            break;
        case E_MSG_TYPE_TUNNEL_DATA:
            handle_transport_message(header, *(tunnel_data_s*) payload.data());
            break;
        default:
            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[%p]::handle_transport_message: Invalid message type!", this);
            break;
    }
}

void node::handle_transport_message(const header_s& header, const id_request_s& payload)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_transport_message: ID request!", this);
}

void node::handle_transport_message(const header_s& header, const id_response_s& payload)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_transport_message: ID response!", this);
}

void node::handle_transport_message(const header_s& header, const link_info_s& payload)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_transport_message: Link info!", this);
}

void node::handle_transport_message(const header_s& header, const link_report_s& payload)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_transport_message: Link report!", this);
}

void node::handle_transport_message(const header_s& header, const route_announce_s& payload)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_transport_message: Route announce!", this);
}

void node::handle_transport_message(const header_s& header, const hub_announce_s& payload)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_transport_message: Hub announce!", this);
}

void node::handle_transport_message(const header_s& header, const n2n_indication_s& payload)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_transport_message: N2N indication!", this);
}

void node::handle_transport_message(const header_s& header, const tunnel_data_s& payload)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_transport_message: Tunnel data!", this);
}

} // namespace bfc_tunnel
