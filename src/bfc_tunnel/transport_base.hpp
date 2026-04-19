#ifndef BFC_TUNNEL_TRANSPORT_HPP
#define BFC_TUNNEL_TRANSPORT_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>

namespace bfc_tunnel
{

struct transport_config_s
{
    std::string address;
    uint16_t port;
};

class transport_base
{
public:
    virtual ~transport_base() = default;
    virtual void send_pdu(const* sockaddr *to, const bfc::buffer_view& pdu) = 0;
    virtual void recv_pdu(sockaddr *from, bfc::buffer_view& pdu) = 0;
    virtual void register_recv_ready_handler(std::function<void()> handler) = 0;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TRANSPORT_HPP