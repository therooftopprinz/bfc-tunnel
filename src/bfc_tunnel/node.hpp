#ifndef BFC_TUNNEL_NODE_HPP
#define BFC_TUNNEL_NODE_HPP

#include <functional>
#include <memory>

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/route_manager.hpp>

namespace bfc_tunnel
{

class node : public std::enable_shared_from_this<node>
{
public:
    node(
        std::shared_ptr<cv_reactor> reactor,
        std::shared_ptr<node_transport_queues_s> transport_queues,
        std::shared_ptr<node_service_queues_s> service_queues
    );
    ~node();

    void initialize();

    void start();
    void stop();

    void on_rcv_pdu(sockaddr *from, const buffer_view& pdu);
    std::function<void(const buffer_view& pdu)> on_send_pdu;

private:
    route_manager rtmgr;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_HPP