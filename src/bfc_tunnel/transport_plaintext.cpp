#include <bfc_tunnel/transport_plaintext.hpp>
#include <bfc_tunnel/logger.hpp>

namespace bfc_tunnel
{

transport_plaintext::transport_plaintext(
    io_reactor_ptr io_reactor,
    cv_reactor_ptr cv_reactor,
    node_transport_queue_ptr in_queue,
    transport_node_queue_ptr out_queue)
    : io_reactor(io_reactor)
    , cv_reactor(cv_reactor)
    , in_queue(in_queue)
    , out_queue(out_queue)
{}

transport_plaintext::~transport_plaintext()
{
}

void transport_plaintext::initialize()
{
    cv_reactor->wake_up(
            [w = weak_from_this()] ()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(tlogger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "transport_plaintext[%p]::initialize: Weak pointer expired!", nullptr);
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
                                log(tlogger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "transport_plaintext[%p]::on_in_queue_ready: Weak pointer expired!", nullptr);
                            }
                        }
                    );
                t->state = E_TRANSPORT_STATE_INITIALIZED;
            }
        );
}

void transport_plaintext::configure(const transport_config_s& config)
{
    io_reactor.wake_up(
            [config, w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(tlogger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "transport_plaintext[%p]::configure: Weak pointer expired!", nullptr);
                    return;
                }

                t->socket = bfc::socket::create_udp4();
                if (!t->socket.is_valid())
                {
                    log(tlogger, E_LOG_BIT_ERROR, "transport_plaintext[%p]::configure: Failed to create socket! error=%d(%s)", t.get(), errno, strerror(errno));
                    return;
                }
                auto bind_addr = bfc::ip4_port_to_sockaddr(config.address, config.port);
                if (!t->socket.bind(bind_addr))
                {
                    log(tlogger, E_LOG_BIT_ERROR, "transport_plaintext[%p]::configure: Failed to bind socket! error=%d(%s)", t.get(), errno, strerror(errno));
                    return;
                }
                t->io_reactor->register_read_ready_handler(t->socket.get_fd(),
                    [w]()
                    {
                        auto t = w.lock();
                        if (!t)
                        {
                            log(tlogger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "transport_plaintext[%p]::on_recv_ready: Weak pointer expired!", nullptr);
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
void transport_plaintext::uninitialize()
{
    if (E_TRANSPORT_STATE_UNINITIALIZED == state)
    {
        return;
    }

    std::promise<void> cv_reactor_promise;
    std::promise<void> io_reactor_promise;

    cv_reactor.wake_up(
            [&cv_reactor_promise, w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(tlogger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "transport_plaintext[%p]::uninitialize: cv_reactor callback: Weak pointer expired!", nullptr);
                    cv_reactor_promise.set_value();
                    return;
                }

                t->io_reactor->unregister_read_ready_handler(t->socket.get_fd());
                t->socket.close();
                cv_reactor_promise.set_value();
            }
        );
    io_reactor.wake_up(
            [&io_reactor_promise, w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(tlogger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "transport_plaintext[%p]::uninitialize: io_reactor callback: Weak pointer expired!", nullptr);
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

void transport_plaintext::configure(const transport_config_s& config)
{
    this->config = config;
    state = E_TRANSPORT_STATE_CONFIGURED;
}

void transport_plaintext::on_in_queue_ready()
{
    auto pdu = in_queue.pop();
    if (pdu.empty())
    {
        return;
    }

    socket.send(pdu, 0, &pdu.addr, sizeof(pdu.addr));
}

void transport_plaintext::on_recv_ready()
{
    sockaddr_storage addr;
    // todo: use buffer pool
    constexpr size_t MAX_PDU_SIZE = 1024*64;
    bfc::buffer pdu(new uint8_t[MAX_PDU_SIZE], MAX_PDU_SIZE);
    auto n = socket.recv(pdu, 0, &addr, sizeof(addr));
    if (n > 0)
    {
        out_queue.push(node_io_pdu_s{addr, std::move(pdu)});
    }
}

} // namespace bfc_tunnel
