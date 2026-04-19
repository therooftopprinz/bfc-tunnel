#ifndef BFC_TUNNEL_TRANSPORT_PLAINTEXT_HPP
#define BFC_TUNNEL_TRANSPORT_PLAINTEXT_HPP

#include <memory>

#include <bfc/default_reactor.hpp>
#include <bfc/cv_reactor.hpp>

#include <bfc_tunnel/bfc_tunnel_types.hpp>

namespace bfc_tunnel
{

struct transport_plaintext_config_s
{
    std::string address;
    uint16_t port;
};

class transport_plaintext :
    public std::enable_shared_from_this<transport_plaintext>
{
public:
    transport_plaintext(
        io_reactor_ptr_t io_reactor,
        cv_reactor_ptr_t cv_reactor,
        node_transport_queue_ptr_t in_queue,
        transport_node_queue_ptr_t out_queue
    );
    ~transport_plaintext();

    void initialize();
    void uninitialize();
    void configure(const transport_plaintext_config_s& config);

private:
    void on_in_queue_ready();
    void on_recv_ready();

    io_reactor_ptr_t io_reactor;
    cv_reactor_ptr_t cv_reactor;
    node_transport_queue_ptr_t in_queue;
    transport_node_queue_ptr_t out_queue;

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

#endif // BFC_TUNNEL_TRANSPORT_PLAINTEXT_HPP