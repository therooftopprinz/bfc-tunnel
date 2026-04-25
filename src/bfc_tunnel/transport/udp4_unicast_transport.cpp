#include <bfc/socket.hpp>
#include <bfc_tunnel/transport/udp4_unicast_transport.hpp>
#include <bfc_tunnel/logger.hpp>

#include <future>
#include <algorithm>
#include <unistd.h>

namespace bfc_tunnel
{

udp4_unicast_transport::udp4_unicast_transport(
    io_reactor_ptr_t io_reactor,
    cv_reactor_ptr_t cv_reactor,
    transport_in_queue_t in_queue,
    transport_out_queue_t out_queue)
    : io_reactor(io_reactor)
    , cv_reactor(cv_reactor)
    , in_queue(in_queue)
    , out_queue(out_queue)
{}

udp4_unicast_transport::~udp4_unicast_transport()
{}

void udp4_unicast_transport::initialize()
{
    cv_reactor->wake_up(
            [w = weak_from_this()] ()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp4_unicast_transport[nullptr]::initialize: Weak pointer expired!");
                    return;
                }

                t->cv_reactor->add_read_rdy(*t->in_queue,
                        [w]()
                        {
                            if (auto t = w.lock())
                            {
                                t->on_in_queue_ready();
                            }
                            else
                            {
                                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp4_unicast_transport[nullptr]::on_in_queue_ready: Weak pointer expired!");
                            }
                        }
                    );
                t->state = E_TRANSPORT_STATE_INITIALIZED;
            }
        );
}

void udp4_unicast_transport::configure(const udp4_unicast_transport_config_s& config)
{
    io_reactor->wake_up(
            [config, w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp4_unicast_transport[nullptr]::configure: Weak pointer expired!");
                    return;
                }

                t->socket = bfc::create_udp4();
                if (0 > t->socket.fd())
                {
                    log(*g_logger, E_LOG_BIT_ERROR, "udp4_unicast_transport[%p]::configure: Failed to create socket! error=%d(%s)", t.get(), errno, strerror(errno));
                    return;
                }
                auto bind_addr = bfc::ip4_port_to_sockaddr(config.address, config.port);
                if (!t->socket.bind(bind_addr))
                {
                    log(*g_logger, E_LOG_BIT_ERROR, "udp4_unicast_transport[%p]::configure: Failed to bind socket! error=%d(%s)", t.get(), errno, strerror(errno));
                    return;
                }
                t->io_reactor->add_read_rdy(t->socket.fd(),
                    [w]()
                    {
                        auto t = w.lock();
                        if (!t)
                        {
                            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp4_unicast_transport[nullptr]::on_recv_ready: Weak pointer expired!");
                            return;
                        }
                        t->on_recv_ready();
                    });
                t->state = E_TRANSPORT_STATE_CONFIGURED;
            }
        );
}

// contracts:
// - io_reactor and cv_reactor are running if initialized or configured
void udp4_unicast_transport::uninitialize()
{
    if (E_TRANSPORT_STATE_UNINITIALIZED == state)
    {
        return;
    }

    std::promise<void> cv_reactor_promise;
    std::promise<void> io_reactor_promise;

    cv_reactor->wake_up(
            [&cv_reactor_promise, w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp4_unicast_transport[nullptr]::uninitialize: cv_reactor callback: Weak pointer expired!");
                    cv_reactor_promise.set_value();
                    return;
                }

                t->io_reactor->rem_read_rdy(t->socket.fd());
                t->socket = bfc::socket();
                cv_reactor_promise.set_value();
            }
        );
    io_reactor->wake_up(
            [&io_reactor_promise, w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp4_unicast_transport[nullptr]::uninitialize: io_reactor callback: Weak pointer expired!");
                    io_reactor_promise.set_value();
                    return;
                }
                t->state = E_TRANSPORT_STATE_UNINITIALIZED;
                io_reactor_promise.set_value();
            }
        );

    cv_reactor_promise.get_future().wait();
    io_reactor_promise.get_future().wait();
}

void udp4_unicast_transport::on_in_queue_ready()
{
    auto ins = in_queue->pop();
    if (ins.empty())
    {
        return;
    }

    for (auto& in : ins)
    {
        socket.send(in.pdu, 0, (sockaddr*) &in.addr, std::min(in.addr_len, (socklen_t) sizeof(in.addr)));
    }
}

void udp4_unicast_transport::on_recv_ready()
{
    sockaddr_storage addr;
    // todo: use buffer pool
    constexpr size_t MAX_PDU_SIZE = 1024*64;
    bfc::buffer pdu(new std::byte[MAX_PDU_SIZE], MAX_PDU_SIZE*sizeof(std::byte), [](const void* p) { delete[] (std::byte*) p; });
    socklen_t addr_len = 0;
    auto n = socket.recv(pdu, 0, (sockaddr*) &addr, &addr_len);
    if (n > 0)
    {
        out_queue->push(node_io_pdu_s{addr, addr_len, std::move(pdu)});
    }
}

} // namespace bfc_tunnel
