#include <bfc_tunnel/node.hpp>
#include <bfc_tunnel/utils/cv_reactor_helper.hpp>
#include <bfc_tunnel/utils/logger.hpp>

#include <cinttypes>
#include <stdexcept>

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
            port_id_counter = 0;
            ports.clear();
            port_refs.clear();
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

            for (auto& transport : t->transports)
            {
                t->cv_reactor->remove_read_rdy(transport->out);
            }
            t->transports.clear();
            t->state = E_NODE_STATE_UNINITIALIZED;
        }
    );
}

void node::add_transport(transport_queue_pair_ptr_t transport)
{
    if (!transport)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::add_transport: Transport queue pair is null!", this);
        return;
    }

    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this(), transport]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::add_transport: Weak pointer expired!");
                return;
            }

            auto port_id = t->port_id_counter++;
            auto port = std::make_shared<port>(t->cv_reactor, port_id);
            t->ports.emplace(port_id, port);
            t->port_refs.emplace(transport.get(), port);
            port->initialize(transport);
        }
    );
}
void node::handle_pdu(const bfc::const_buffer_view& pdu)
{
    if (pdu.empty())
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_pdu: Empty PDU!", this);
        return;
    }

    try
    {
        cum::BTPMessage msg;
        cum::per_codec_ctx ctx(const_cast<std::byte*>(pdu.data()), pdu.size());
        cum::decode_per(msg, ctx);
        handle_btp_message(msg);
    }
    catch (const std::exception& e)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_pdu: PER decode failed: %s", this, e.what());
    }
}

void node::handle_btp_message(const cum::BTPMessage& msg)
{
    std::visit([this](const auto& m) { handle_btp_message(m); }, msg);
}

void node::handle_btp_message(const cum::beacon& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Beacon!", this);
}

void node::handle_btp_message(const cum::msg1& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Msg1!", this);
}

void node::handle_btp_message(const cum::msg2& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Msg2!", this);
}

void node::handle_btp_message(const cum::link_info& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Link info!", this);
}

void node::handle_btp_message(const cum::link_report& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Link report!", this);
}

void node::handle_btp_message(const cum::route_announce& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: Route announce!", this);
}

void node::handle_btp_message(const cum::n2n_indication& /*msg*/)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_btp_message: N2N indication!", this);
}

} // namespace bfc_tunnel
