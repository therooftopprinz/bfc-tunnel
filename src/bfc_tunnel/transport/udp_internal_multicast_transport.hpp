#ifndef BFC_TUNNEL_TRANSPORT_udp_internal_multicast_transport_HPP
#define BFC_TUNNEL_TRANSPORT_udp_internal_multicast_transport_HPP

#include <memory>
#include <future>
#include <optional>

#include <netinet/in.h>

#include <bfc/default_reactor.hpp>
#include <bfc/cv_reactor.hpp>

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>

namespace bfc_tunnel
{

struct udp_internal_multicast_transport_config_s
{
    std::string group;
    uint16_t    port = 0;
    std::string interface;
};

class udp_internal_multicast_transport : public std::enable_shared_from_this<udp_internal_multicast_transport>
{
public:
    udp_internal_multicast_transport(
        io_reactor_ptr_t io_reactor, cv_reactor_ptr_t cv_reactor,
        transport_queue_pair_ptr_t transport_queue_pair
    );
    ~udp_internal_multicast_transport();

    void initialize(const udp_internal_multicast_transport_config_s& config);
    void deinitialize();

private:
    void on_in_queue_ready();
    void on_sock_recv_ready();

    void handle(const transport_data_s& data);

    io_reactor_ptr_t io_reactor;
    cv_reactor_ptr_t cv_reactor;
    transport_queue_pair_ptr_t transport_queue_pair;

    bfc::socket sock;
    sockaddr_in group_addr{};
    ip_mreq mreq{};
    udp_internal_multicast_transport_config_s config;

    enum transport_state_e
    {
        E_TRANSPORT_STATE_UNINITIALIZED,
        E_TRANSPORT_STATE_INITIALIZED
    };

    std::atomic<transport_state_e> state = E_TRANSPORT_STATE_UNINITIALIZED;

    std::optional<std::promise<void>> deinitialize_promise;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_udp_internal_multicast_transport_HPP
