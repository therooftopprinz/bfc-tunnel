#include <bfc_tunnel/port.hpp>
#include <bfc_tunnel/utils/cv_reactor_helper.hpp>
#include <bfc_tunnel/utils/logger.hpp>

namespace bfc_tunnel
{

port::port(cv_reactor_ptr_t cv_reactor, port_id_t port_id)
    : port_id(port_id)
    , cv_reactor(std::move(cv_reactor))
{}

port::~port()
{
    uninitialize();
}

void port::initialize(transport_queue_pair_ptr_t transport)
{
    if (!transport)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "port[%u]::initialize: Transport queue pair is null!", port_id);
        return;
    }

    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this(), transport]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "port[nullptr]::initialize: Weak pointer expired!");
                return;
            }

            t->transport = transport;
            t->cv_reactor->add_read_rdy(
                t->transport->out,
                [w]()
                {
                    auto t = w.lock();
                    if (!t)
                    {
                        log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "port[nullptr]::on_out_queue_ready: Weak pointer expired!");
                        return;
                    }
                    t->on_transport_out_ready();
                }
            );
            t->state = E_PORT_STATE_INITIALIZED;
        }
    );
}

void port::uninitialize()
{
    utils::dispatch_sync(*cv_reactor,
        [w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t)
            {
                return;
            }

            t->transport.reset();
        }
    );
}

void port::on_transport_out_ready()
{
    auto outs = transport->out.pop();
    if (outs.empty())
    {
        return;
    }
}
    for (auto& out : outs)
    {
        std::visit([this](auto& e) { handle_transport_out(e); }, out);
    }
}

} // namespace bfc_tunnel
