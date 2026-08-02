#ifndef BFC_TUNNEL_TRANSPORT_udp_external_multicast_transport_HPP
#define BFC_TUNNEL_TRANSPORT_udp_external_multicast_transport_HPP

#include <memory>
#include <future>
#include <optional>

#include <bfc/default_reactor.hpp>
#include <bfc/cv_reactor.hpp>

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>

namespace bfc_tunnel
{

// Client-side transport for an external multicaster: raw BTF payloads are
// injected into the multicaster via `send` (host:port) and received back
// from the multicast fanout on the locally bound `recv` (host:port). The
// multicaster process itself is out of scope for this transport.
struct udp_external_multicast_transport_config_s
{
    std::string send;
    std::string recv;
};

class udp_external_multicast_transport : public std::enable_shared_from_this<udp_external_multicast_transport>
{
public:
    udp_external_multicast_transport(
        io_reactor_ptr_t io_reactor, cv_reactor_ptr_t cv_reactor,
        transport_queue_pair_ptr_t transport_queue_pair
    );
    ~udp_external_multicast_transport();

    void initialize(const udp_external_multicast_transport_config_s& config);
    void deinitialize();

private:
    void on_in_queue_ready();
    void on_sock_recv_ready();

    void handle(const transport_data_s& data);

    io_reactor_ptr_t io_reactor;
    cv_reactor_ptr_t cv_reactor;
    transport_queue_pair_ptr_t transport_queue_pair;

    bfc::socket sock;
    bool is_v6 = false;
    sockaddr_t send_address = sockaddr_none{};
    udp_external_multicast_transport_config_s config;

    enum transport_state_e
    {
        E_TRANSPORT_STATE_UNINITIALIZED,
        E_TRANSPORT_STATE_INITIALIZED
    };

    std::atomic<transport_state_e> state = E_TRANSPORT_STATE_UNINITIALIZED;

    std::optional<std::promise<void>> deinitialize_promise;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_udp_external_multicast_transport_HPP
