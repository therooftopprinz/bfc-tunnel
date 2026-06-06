#ifndef BFC_TUNNEL_TRANSPORT_udp_unicast_transport_HPP
#define BFC_TUNNEL_TRANSPORT_udp_unicast_transport_HPP

#include <memory>

#include <bfc/default_reactor.hpp>
#include <bfc/cv_reactor.hpp>

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>

namespace bfc_tunnel
{

struct udp_unicast_transport_config_s
{
    std::string address;
};

class udp_unicast_transport : public std::enable_shared_from_this<udp_unicast_transport>
{
public:
    udp_unicast_transport(
        io_reactor_ptr_t io_reactor, cv_reactor_ptr_t cv_reactor,
        transport_queue_pair_ptr_t transport_queue_pair
    );
    ~udp_unicast_transport();

    void initialize();
    void deinitialize();
    void configure(const udp_unicast_transport_config_s& config);

private:
    void on_in_queue_ready();
    void on_sock_recv_ready();

    void handle(const transport_data_s& data);
    void handle(const transport4_data_s& data);
    void handle(const transport6_data_s& data);

    io_reactor_ptr_t io_reactor;
    cv_reactor_ptr_t cv_reactor;
    transport_queue_pair_ptr_t transport_queue_pair;

    bfc::socket sock;
    bool is_v6 = false;

    enum transport_state_e
    {
        E_TRANSPORT_STATE_UNINITIALIZED,
        E_TRANSPORT_STATE_INITIALIZED,
        E_TRANSPORT_STATE_CONFIGURED
    };

    transport_state_e state = E_TRANSPORT_STATE_UNINITIALIZED;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_udp_unicast_transport_HPP
