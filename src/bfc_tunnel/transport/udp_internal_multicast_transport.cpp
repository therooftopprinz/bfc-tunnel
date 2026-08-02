#include <bfc_tunnel/transport/transport_types.hpp>

#include <bfc/socket.hpp>
#include <bfc/sized_buffer.hpp>
#include <bfc_tunnel/transport/udp_internal_multicast_transport.hpp>
#include <bfc_tunnel/utils/logger.hpp>
#include <bfc_tunnel/utils/reactor_helper.hpp>
#include <bfc_tunnel/utils/string_utils.hpp>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <future>
#include <unistd.h>

namespace bfc_tunnel
{

udp_internal_multicast_transport::udp_internal_multicast_transport(
    io_reactor_ptr_t io_reactor,
    cv_reactor_ptr_t cv_reactor,
    transport_queue_pair_ptr_t transport_queue_pair)
    : io_reactor(io_reactor)
    , cv_reactor(cv_reactor)
    , transport_queue_pair(transport_queue_pair)
{}

udp_internal_multicast_transport::~udp_internal_multicast_transport()
{
    deinitialize();
}

void udp_internal_multicast_transport::initialize(const udp_internal_multicast_transport_config_s& config)
{
    utils::dispatch_sync(*io_reactor,
        [config, w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_internal_multicast_transport[nullptr]::initialize: udp_internal_multicast_transport weak pointer is expired!");
                return;
            }

            if (E_TRANSPORT_STATE_UNINITIALIZED != t->state)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::initialize: Transport already initialized!", t.get());
                return;
            }

            t->sock = bfc::create_udp4();
            if (0 > t->sock.fd())
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::initialize: Failed to create socket! error=%d(%s)", t.get(), errno, strerror(errno));
                return;
            }

            if (0 > t->sock.set_sock_opt(SOL_SOCKET, SO_REUSEADDR, int{1}))
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::initialize: Failed to set SO_REUSEADDR! error=%d(%s)", t.get(), errno, strerror(errno));
                t->sock = {};
                return;
            }

            const std::string bind_interface = config.interface.empty() ? "0.0.0.0" : config.interface;

            auto bind_addr = bfc::ip4_port_to_sockaddr(bind_interface, config.port);
            if (AF_INET != bind_addr.sin_family)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::initialize: Invalid interface address! interface=%s",
                    t.get(), bind_interface.c_str());
                t->sock = {};
                return;
            }

            if (0 > t->sock.bind(bind_addr))
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::initialize: Failed to bind socket! error=%d(%s)",
                    t.get(), errno, strerror(errno));
                t->sock = {};
                return;
            }

            auto group_sockaddr = bfc::ip4_port_to_sockaddr(config.group, config.port);
            if (AF_INET != group_sockaddr.sin_family)
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::initialize: Invalid group address! group=%s",
                    t.get(), config.group.c_str());
                t->sock = {};
                return;
            }

            ip_mreq mreq{};
            mreq.imr_multiaddr = group_sockaddr.sin_addr;
            mreq.imr_interface = bind_addr.sin_addr;

            if (0 > t->sock.set_sock_opt(IPPROTO_IP, IP_ADD_MEMBERSHIP, mreq))
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::initialize: Failed to join multicast group! group=%s error=%d(%s)",
                    t.get(), config.group.c_str(), errno, strerror(errno));
                t->sock = {};
                return;
            }

            const int flags = fcntl(t->sock.fd(), F_GETFL, 0);
            if (0 > flags || 0 > fcntl(t->sock.fd(), F_SETFL, flags | O_NONBLOCK))
            {
                log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::initialize: Failed to set O_NONBLOCK! error=%d(%s)",
                    t.get(), errno, strerror(errno));
                t->sock.set_sock_opt(IPPROTO_IP, IP_DROP_MEMBERSHIP, mreq);
                t->sock = {};
                return;
            }

            t->group_addr = group_sockaddr;
            t->mreq = mreq;

            utils::dispatch_sync(*t->cv_reactor,
                [w]()
                {
                    auto t = w.lock();
                    if (!t)
                    {
                        log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_internal_multicast_transport[nullptr]::initialize: cv_reactor callback: udp_internal_multicast_transport weak pointer is expired!");
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
                                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_internal_multicast_transport[nullptr]::on_in_queue_ready: udp_internal_multicast_transport weak pointer is expired!");
                            }
                        }
                    );
                }
            );

            t->io_reactor->add_read_rdy(
                    t->sock.fd(),
                    [w]()
                    {
                        auto t = w.lock();
                        if (!t)
                        {
                            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_internal_multicast_transport[nullptr]::on_recv_ready: udp_internal_multicast_transport weak pointer is expired!");
                            return;
                        }
                        t->on_sock_recv_ready();
                    }
                );

            t->config = config;
            t->state = E_TRANSPORT_STATE_INITIALIZED;
        }
    );
}

void udp_internal_multicast_transport::deinitialize()
{
    deinitialize_promise.emplace();
    std::promise<void>& done = deinitialize_promise.value();

    utils::dispatch_sync(*io_reactor,
            [w = weak_from_this(), &done]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_internal_multicast_transport[nullptr]::deinitialize: io_reactor callback: udp_internal_multicast_transport weak pointer is expired!");
                    done.set_value();
                    return;
                }

                if (E_TRANSPORT_STATE_UNINITIALIZED == t->state.load())
                {
                    log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::deinitialize: Transport already uninitialized!", t.get());
                    done.set_value();
                    t->deinitialize_promise.reset();
                    return;
                }

                const int sock_fd = t->sock.fd();

                t->state.store(E_TRANSPORT_STATE_UNINITIALIZED);

                if (0 > t->sock.set_sock_opt(IPPROTO_IP, IP_DROP_MEMBERSHIP, t->mreq))
                {
                    log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::deinitialize: Failed to leave multicast group! error=%d(%s)",
                        t.get(), errno, strerror(errno));
                }

                utils::dispatch_sync(*t->cv_reactor,
                    [w]()
                    {
                        if (auto t = w.lock())
                        {
                            t->cv_reactor->remove_read_rdy(t->transport_queue_pair->in);
                        }
                        else
                        {
                            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_internal_multicast_transport[nullptr]::deinitialize: cv_reactor remove_read_rdy: udp_internal_multicast_transport weak pointer is expired!");
                        }
                    }
                );

                t->io_reactor->rem_read_rdy(
                    sock_fd,
                    [w]()
                    {
                        if (auto t = w.lock())
                        {
                            t->sock = bfc::socket();
                            t->config = {};
                            t->group_addr = {};
                            t->mreq = {};

                            if (t->deinitialize_promise)
                            {
                                t->deinitialize_promise->set_value();
                                t->deinitialize_promise.reset();
                            }
                        }
                        else
                        {
                            log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_internal_multicast_transport[nullptr]::deinitialize: rem_read_rdy: udp_internal_multicast_transport weak pointer is expired!");
                        }
                    });
            }
        );

    deinitialize_promise->get_future().wait();
    deinitialize_promise.reset();
}

void udp_internal_multicast_transport::on_in_queue_ready()
{
    auto ins = transport_queue_pair->in.pop();
    if (ins.empty())
    {
        return;
    }

    for (auto& in : ins)
    {
        handle(in);
    }
}

void udp_internal_multicast_transport::handle(const transport_data_s& data)
{
    if (state.load() != E_TRANSPORT_STATE_INITIALIZED)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::handle: Transport not initialized!", this);
        transport_queue_pair->out.push(transport_delivery_failure{data.id, data.address, EBADF});
        return;
    }

    auto send_error = std::visit(
        [this, &data](const auto& addr) -> std::optional<int>
        {
            using addr_t = std::decay_t<decltype(addr)>;
            if constexpr (std::is_same_v<addr_t, sockaddr_none>)
            {
                if (0 > sock.send(data.data, 0, (sockaddr*)&group_addr, sizeof(group_addr)))
                {
                    return errno;
                }
                return std::nullopt;
            }
            else if constexpr (std::is_same_v<addr_t, sockaddr_in6>)
            {
                return EAFNOSUPPORT;
            }
            else if (0 > sock.send(data.data, 0, (sockaddr*)&addr, sizeof(addr)))
            {
                return errno;
            }
            return std::nullopt;
        },
        data.address
    );

    if (send_error)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::handle: Failed to send data! error=%d(%s)", this, *send_error, strerror(*send_error));
        transport_queue_pair->out.push(transport_delivery_failure{data.id, data.address, *send_error});
    }
}

void udp_internal_multicast_transport::on_sock_recv_ready()
{
    // todo: use buffer pool
    constexpr size_t MAX_PDU_SIZE = 1024 * 64;

    while (state.load() == E_TRANSPORT_STATE_INITIALIZED)
    {
        sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        bfc::sized_buffer pdu(MAX_PDU_SIZE);

        auto n = sock.recv(pdu, 0, (sockaddr*)&addr, &addr_len);
        if (n > 0)
        {
            pdu.resize(static_cast<size_t>(n));
            transport_queue_pair->out.push(transport_data_s{0, addr, std::move(pdu)});
            continue;
        }

        if (0 == n || EAGAIN == errno || EWOULDBLOCK == errno)
        {
            break;
        }

        log(*g_logger, E_LOG_BIT_ERROR, "udp_internal_multicast_transport[%p]::on_sock_recv_ready: Failed to recv data! error=%d(%s)", this, errno, strerror(errno));
        break;
    }
}

} // namespace bfc_tunnel
