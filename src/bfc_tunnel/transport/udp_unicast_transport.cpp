#include <bfc_tunnel/transport/transport_types.hpp>

#include <bfc/socket.hpp>
#include <bfc/sized_buffer.hpp>
#include <bfc_tunnel/transport/udp_unicast_transport.hpp>
#include <bfc_tunnel/utils/logger.hpp>
#include <bfc_tunnel/utils/string_utils.hpp>

#include <numeric>
#include <future>
#include <unistd.h>

namespace bfc_tunnel
{

udp_unicast_transport::udp_unicast_transport(
    io_reactor_ptr_t io_reactor,
    cv_reactor_ptr_t cv_reactor,
    transport_queue_pair_ptr_t transport_queue_pair)
    : io_reactor(io_reactor)
    , cv_reactor(cv_reactor)
    , transport_queue_pair(transport_queue_pair)
{}

udp_unicast_transport::~udp_unicast_transport()
{
    deinitialize();
}

void udp_unicast_transport::initialize()
{
    io_reactor->wake_up(
        [w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_unicast_transport[nullptr]::initialize: Weak pointer expired!");
                return;
            }

            if (E_TRANSPORT_STATE_UNINITIALIZED != t->state)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::initialize: Transport already initialized!", t.get());
                return;
            }

            t->cv_reactor->add_read_rdy(
                    t->transport_queue_pair->in,
                    [w]()
                    {
                        if (auto t = w.lock())
                        {
                            t->on_in_queue_ready();
                        }
                        else
                        {
                            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_unicast_transport[nullptr]::on_in_queue_ready: Weak pointer expired!");
                        }
                    }
                );

            t->state = E_TRANSPORT_STATE_INITIALIZED;
        }
    );
}

void udp_unicast_transport::configure(const udp_unicast_transport_config_s& config)
{
    io_reactor->wake_up(
        [config, w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_unicast_transport[nullptr]::configure: Weak pointer expired!");
                return;
            }

            if (E_TRANSPORT_STATE_INITIALIZED != t->state)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::configure: Transport not initialized!", t.get());
                return;
            }

            t->sock = {};
            t->config = {};

            t->is_v6 = std::accumulate(config.address.begin(), config.address.end(), 0, [](int acc, char c) { return acc + (c == ':'); }) > 1;
            if (t->is_v6)
            {
                t->sock = bfc::create_udp6();
            }
            else
            {
                t->sock = bfc::create_udp4();
            }

            if (0 > t->sock.fd())
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::configure: Failed to create socket! error=%d(%s)", t.get(), errno, strerror(errno));
                return;
            }

            auto split_ip_and_port = [](const std::string& address) -> std::optional<std::pair<std::string, uint16_t>> {
                auto pos = address.find_last_of(':');
                if (pos == std::string::npos)
                {
                    return std::make_pair(address, uint16_t{0});
                }

                auto port = utils::str_as<uint16_t>(address.substr(pos + 1));
                if (!port)
                {
                    return std::nullopt;
                }

                return std::make_pair(address.substr(0, pos), *port);
            };

            auto parsed = split_ip_and_port(config.address);
            if (!parsed)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::configure: Invalid address port! address=%s",
                    t.get(), config.address.c_str());
                t->sock = {};
                return;
            }

            const auto& [ip_address, port] = *parsed;

            if (t->is_v6)
            {
                auto bind_addr = bfc::ip6_port_to_sockaddr(ip_address, port);
                if (0 > t->sock.bind(bind_addr))
                {
                    log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::configure: Failed to bind6 socket! error=%d(%s)",
                        t.get(), errno, strerror(errno));
                    t->sock = {};
                    return;
                }
            }
            else
            {
                auto bind_addr = bfc::ip4_port_to_sockaddr(ip_address, port);
                if (0 > t->sock.bind(bind_addr))
                {
                    log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::configure: Failed to bind4 socket! error=%d(%s)",
                        t.get(), errno, strerror(errno));
                    t->sock = {};
                    return;
                }
            }

            t->io_reactor->add_read_rdy(
                    t->sock.fd(),
                    [w]()
                    {
                        auto t = w.lock();
                        if (!t)
                        {
                            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_unicast_transport[nullptr]::on_recv_ready: Weak pointer expired!");
                            return;
                        }
                        t->on_sock_recv_ready();
                    }
                );
            t->config = config;
            t->transport_queue_pair->out.push(transport_identity_s{
                E_TRANSPORT_TYPE_UDP_UNICAST,
                config.address
            });
            t->state = E_TRANSPORT_STATE_CONFIGURED;
        }
    );
}

void udp_unicast_transport::deinitialize()
{
    deinitialize_promise.emplace();
    std::promise<void>& done = deinitialize_promise.value();

    io_reactor->wake_up(
            [w = weak_from_this(), &done]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_unicast_transport[nullptr]::deinitialize: io_reactor callback: Weak pointer expired!");
                    done.set_value();
                    return;
                }

                if (E_TRANSPORT_STATE_UNINITIALIZED == t->state.load())
                {
                    log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::deinitialize: Transport already uninitialized!", t.get());
                    done.set_value();
                    t->deinitialize_promise.reset();
                    return;
                }

                const int sock_fd = t->sock.fd();

                t->state.store(E_TRANSPORT_STATE_UNINITIALIZED);
                t->cv_reactor->remove_read_rdy(t->transport_queue_pair->in);
                t->io_reactor->rem_read_rdy(
                    sock_fd,
                    [w]()
                    {
                        if (auto t = w.lock())
                        {
                            t->sock = bfc::socket();
                            t->config = {};

                            if (t->deinitialize_promise)
                            {
                                t->deinitialize_promise->set_value();
                                t->deinitialize_promise.reset();
                            }
                        }
                    });
            }
        );

    deinitialize_promise->get_future().wait();
    deinitialize_promise.reset();
}

void udp_unicast_transport::on_in_queue_ready()
{
    auto ins = transport_queue_pair->in.pop();
    if (ins.empty())
    {
        return;
    }

    for (auto& in : ins)
    {
        std::visit([this](auto& e) {handle(e);}, in);
    }
}

void udp_unicast_transport::handle(const transport_query_identity_s& /*data*/)
{
    if (state.load() != E_TRANSPORT_STATE_CONFIGURED)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::handle: Transport not configured!", this);
        transport_queue_pair->out.push(transport_identity_s{
            E_TRANSPORT_TYPE_UDP_UNICAST,
            ""
        });
        return;
    }

    transport_queue_pair->out.push(transport_identity_s{
        E_TRANSPORT_TYPE_UDP_UNICAST,
        config.address
    });
}

void udp_unicast_transport::handle(const transport_data_s& /*data*/)
{
    log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::handle: Unsupported transport data!", this);
}

void udp_unicast_transport::handle(const transport4_data_s& data)
{
    if (state.load() != E_TRANSPORT_STATE_CONFIGURED)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::handle: Transport not configured!", this);
        transport_queue_pair->out.push(transport_delivery_failure{data.id, data.address, EBADF});
        return;
    }
    if (0 > sock.send(data.data, 0, (sockaddr*)&data.address, sizeof(data.address)))
    {
        log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::handle: Failed to send data! error=%d(%s)", this, errno, strerror(errno));
        transport_queue_pair->out.push(transport_delivery_failure{data.id, data.address, errno});
        return;
    }
}

void udp_unicast_transport::handle(const transport6_data_s& data)
{
    if (state.load() != E_TRANSPORT_STATE_CONFIGURED)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::handle: Transport not configured!", this);
        transport_queue_pair->out.push(transport_delivery_failure{data.id, data.address, EBADF});
        return;
    }
    if (0 > sock.send(data.data, 0, (sockaddr*)&data.address, sizeof(data.address)))
    {
        log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::handle: Failed to send data! error=%d(%s)", this, errno, strerror(errno));
        transport_queue_pair->out.push(transport_delivery_failure{data.id, data.address, errno});
        return;
    }
}

void udp_unicast_transport::on_sock_recv_ready()
{
    // todo: use buffer pool
    constexpr size_t MAX_PDU_SIZE = 1024 * 64;

    while (state.load() == E_TRANSPORT_STATE_CONFIGURED)
    {
        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        bfc::sized_buffer pdu(MAX_PDU_SIZE);

        auto n = sock.recv(pdu, 0, (sockaddr*)&addr, &addr_len);
        if (n > 0)
        {
            pdu.resize(static_cast<size_t>(n));
            auto buffer = std::move(pdu).to_buffer();
            if (is_v6)
            {
                transport_queue_pair->out.push(transport6_data_s{0, *(sockaddr_in6*)&addr, std::move(buffer)});
            }
            else
            {
                transport_queue_pair->out.push(transport4_data_s{0, *(sockaddr_in*)&addr, std::move(buffer)});
            }
            continue;
        }

        if (0 == n || EAGAIN == errno || EWOULDBLOCK == errno)
        {
            break;
        }

        log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::on_sock_recv_ready: Failed to recv data! error=%d(%s)", this, errno, strerror(errno));
        break;
    }
}

} // namespace bfc_tunnel
