#include <bfc_tunnel/node.hpp>
#include <bfc_tunnel/logger.hpp>
#include <bfc_tunnel/protocol.hpp>

namespace bfc_tunnel
{

node::node(
    cv_reactor_ptr cv_reactor,
    node_transport_queue_ptr transport_out,
    transport_node_queue_ptr transport_in,
    node_service_queue_ptr service_out,
    service_node_queue_ptr service_in)
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
    cv_reactor.wake_up(
            [w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(tlogger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[%p]::initialize: Weak pointer expired!", nullptr);
                    return;
                }

                t->cv_reactor->add_read_rdy(t->transport_in,
                    [w]()
                    {
                        auto t = w.lock();
                        if (!t)
                        {
                            log(tlogger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[%p]::on_transport_in_queue_ready: Weak pointer expired!", nullptr);
                            return;
                        }
                        t->on_transport_in_queue_ready();
                    });
                t->cv_reactor->add_read_rdy(t->service_in,
                    [w]()
                    {
                        auto t = w.lock();
                        if (!t)
                        {
                            log(tlogger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "node[%p]::on_service_in_queue_ready: Weak pointer expired!", nullptr);
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
    auto pdu = transport_in.pop();
    if (pdu.empty())
    {
        return;
    }

    auto& header = (header_s) pdu.data();
}

void node::on_service_in_queue_ready()
{
    auto sdu = service_in.pop();
    if (sdu.empty())
    {
        return;
    }
}
} // namespace bfc_tunnel