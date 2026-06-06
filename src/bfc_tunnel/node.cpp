#include <bfc_tunnel/node.hpp>
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
    cv_reactor->wake_up(
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
    cv_reactor->wake_up(
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

    cv_reactor->wake_up(
        [w = weak_from_this(), transport]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::add_transport: Weak pointer expired!");
                return;
            }

            t->transports.push_back(transport);
            t->cv_reactor->add_read_rdy(
                transport->out,
                [w, transport]()
                {
                    auto t = w.lock();
                    if (!t)
                    {
                        log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[nullptr]::on_transport_out_ready: Weak pointer expired!");
                        return;
                    }
                    t->on_transport_out_ready(transport);
                }
            );
        }
    );
}

void node::on_transport_out_ready(transport_queue_pair_ptr_t transport)
{
    auto outs = transport->out.pop();
    if (outs.empty())
    {
        return;
    }

    for (auto& out : outs)
    {
        std::visit([this](auto& e) { handle_transport_out(e); }, out);
    }
}

void node::handle_transport_out(const transport_identity_s& data)
{
    log(*g_logger, E_LOG_BIT_INFO, "node[%p]::handle_transport_out: Transport identity type=%d address=%s",
        this, static_cast<int>(data.type), data.address.c_str());
}

void node::handle_transport_out(const transport_data_s& data)
{
    handle_pdu(bfc::const_buffer_view(data.data));
}

void node::handle_transport_out(const transport4_data_s& data)
{
    handle_pdu(bfc::const_buffer_view(data.data));
}

void node::handle_transport_out(const transport6_data_s& data)
{
    handle_pdu(bfc::const_buffer_view(data.data));
}

void node::handle_transport_out(const transport_delivery_failure& data)
{
    log(*g_logger, E_LOG_BIT_ERROR, "node[%p]::handle_transport_out: Delivery failure id=%" PRIu64 " error=%d",
        this, data.id, data.error);
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
