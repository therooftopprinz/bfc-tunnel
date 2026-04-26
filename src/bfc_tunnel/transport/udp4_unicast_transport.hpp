#ifndef BFC_TUNNEL_TRANSPORT_udp4_unicast_transport_HPP
#define BFC_TUNNEL_TRANSPORT_udp4_unicast_transport_HPP

#include <memory>

#include <bfc/default_reactor.hpp>
#include <bfc/cv_reactor.hpp>

#include <bfc_tunnel/transport/transport_types.hpp>

namespace bfc_tunnel
{

struct udp4_unicast_transport_config_s
{
    std::string bind_address;
    std::string peer_cache_file;
    std::vector<std::string> peers;
};

class udp4_unicast_transport :
    public std::enable_shared_from_this<udp4_unicast_transport>
{
public:
    udp4_unicast_transport(
        io_reactor_ptr_t io_reactor,
        cv_reactor_ptr_t cv_reactor,
        trans in_queue,
        transport_node_queue_ptr_t out_queue
    );
    ~udp4_unicast_transport();

    void initialize();
    void uninitialize();
    void configure(const transport_plaintext_config_s& config);

private:
    void on_in_queue_ready();
    void on_recv_ready();

    io_reactor_ptr_t io_reactor;
    cv_reactor_ptr_t cv_reactor;
    transport_in_queue_t in_queue;
    transport_out_queue_t out_queue;

    bfc::socket socket;

    enum transport_state_e
    {
        E_TRANSPORT_STATE_UNINITIALIZED,
        E_TRANSPORT_STATE_INITIALIZED,
        E_TRANSPORT_STATE_CONFIGURED
    };
    transport_state_e state;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_udp4_unicast_transport_HPP
