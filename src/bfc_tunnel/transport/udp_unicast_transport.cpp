#include <bfc_tunnel/transport/transport_types.hpp>

#include <bfc/socket.hpp>
#include <bfc/sized_buffer.hpp>
#include <bfc_tunnel/transport/udp_unicast_transport.hpp>
#include <bfc_tunnel/utils/logger.hpp>

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
    if (E_TRANSPORT_STATE_UNINITIALIZED != state)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::initialize: Transport already initialized!", this);
        return;
    }

    io_reactor->wake_up(
        [w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_unicast_transport[nullptr]::initialize: Weak pointer expired!");
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
    if (E_TRANSPORT_STATE_INITIALIZED != state)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::configure: Transport not initialized!", this);
        return;
    }

    io_reactor->wake_up(
        [config, w = weak_from_this()]()
        {
            auto t = w.lock();
            if (!t)
            {
                log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_unicast_transport[nullptr]::configure: Weak pointer expired!");
                return;
            }

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

            auto split_ip_and_port = [](const std::string& address) -> std::pair<std::string, uint16_t> {
                auto pos = address.find_last_of(':');
                if (pos == std::string::npos)
                {
                    return {address, 0};
                }
                return {address.substr(0, pos), std::stoi(address.substr(pos + 1))};
            };

            if (t->is_v6)
            {
                auto [ipv6_address, port] = split_ip_and_port(config.address);
                auto bind_addr = bfc::ip6_port_to_sockaddr(ipv6_address, port);
                if (0 > t->sock.bind(bind_addr))
                {
                    log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::configure: Failed to bind socket! error=%d(%s)",
                        t.get(), errno, strerror(errno));
                    return;
                }
            }
            else
            {
                auto [ipv4_address, port] = split_ip_and_port(config.address);
                auto bind_addr = bfc::ip4_port_to_sockaddr(ipv4_address, port);
                if (0 > t->sock.bind(bind_addr))
                {
                    log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::configure: Failed to bind6 socket! error=%d(%s)",
                        t.get(), errno, strerror(errno));
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
            t->transport_queue_pair->out.push(transport_config_s{
                E_TRANSPORT_TYPE_UDP_UNICAST,
                config.address
            });
            t->state = E_TRANSPORT_STATE_CONFIGURED;
        }
    );
}

void udp_unicast_transport::deinitialize()
{
    if (E_TRANSPORT_STATE_UNINITIALIZED == state)
    {
        return;
    }

    std::promise<void> reactor_promise;

    io_reactor->wake_up(
            [&reactor_promise, w = weak_from_this()]()
            {
                auto t = w.lock();
                if (!t)
                {
                    log(*g_logger, E_LOG_BIT_SHOULD_NOT_HAPPEN, "udp_unicast_transport[nullptr]::uninitialize: io_reactor callback: Weak pointer expired!");
                    reactor_promise.set_value();
                    return;
                }
                t->io_reactor->rem_read_rdy(t->sock.fd());
                t->cv_reactor->remove_read_rdy(t->transport_queue_pair->in);
                t->sock = bfc::socket();
                t->state = E_TRANSPORT_STATE_UNINITIALIZED;
                reactor_promise.set_value();
            }
        );

        reactor_promise.get_future().wait();
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

void udp_unicast_transport::handle(const transport_data_s& /*data*/)
{
    log(*g_logger, E_LOG_BIT_ERROR, "udp_unicast_transport[%p]::handle: Unsupported transport data!", this);
}

void udp_unicast_transport::handle(const transport4_data_s& data)
{
    sock.send(data.data, 0, (sockaddr*)&data.address, sizeof(data.address));
}

void udp_unicast_transport::handle(const transport6_data_s& data)
{
    sock.send(data.data, 0, (sockaddr*)&data.address, sizeof(data.address));
}

void udp_unicast_transport::on_sock_recv_ready()
{
    sockaddr_storage addr;
    // todo: use buffer pool
    constexpr size_t MAX_PDU_SIZE = 1024 * 64;
    bfc::sized_buffer pdu(MAX_PDU_SIZE);
    socklen_t addr_len = sizeof(addr);
    auto n = sock.recv(pdu, 0, (sockaddr*)&addr, &addr_len);
    if (n > 0)
    {
        pdu.resize(static_cast<size_t>(n));
        auto buffer = std::move(pdu).to_buffer();
        if (is_v6)
        {
            transport_queue_pair->out.push(transport6_data_s{*(sockaddr_in6*)&addr, std::move(buffer)});
        }
        else
        {
            transport_queue_pair->out.push(transport4_data_s{*(sockaddr_in*)&addr, std::move(buffer)});
        }
    }
}

} // namespace bfc_tunnel
