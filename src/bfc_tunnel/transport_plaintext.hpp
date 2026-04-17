#ifndef BFC_TUNNEL_TRANSPORT_PLAINTEXT_HPP
#define BFC_TUNNEL_TRANSPORT_PLAINTEXT_HPP

#include <bfc_tunnel/transport_base.hpp>

namespace bfc_tunnel
{

class transport_plaintext :
    public transport_base,
    public std::enable_shared_from_this<transport_plaintext>
{
public:
    transport_plaintext(io_reactor& reactor);
    ~transport_plaintext();

    void initialize();

    void send_pdu(const* sockaddr *to, const bfc::buffer_view& pdu) override;
    void recv_pdu(sockaddr *from, bfc::buffer_view& pdu) override;
    void register_recv_ready_handler(std::function<void()> handler) override;

private:
    // delegate everthing to 
    void initialize0();
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_PLAINTEXT_HPP