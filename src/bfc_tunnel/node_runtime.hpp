#ifndef BFC_TUNNEL_NODE_RUNTIME_HPP
#define BFC_TUNNEL_NODE_RUNTIME_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/node.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>
#include <bfc_tunnel/transport/udp_external_multicast_transport.hpp>
#include <bfc_tunnel/transport/udp_internal_multicast_transport.hpp>
#include <bfc_tunnel/transport/udp_unicast_transport.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

namespace bfc_tunnel
{

struct transport_entry_s
{
    std::string name;
    std::string type; // UNICAST | INTERNAL_MULTICAST | EXTERNAL_MULTICAST
    std::unordered_map<std::string, std::string> fields;
    port_ptr_t port;
    std::variant<
        std::shared_ptr<udp_unicast_transport>,
        std::shared_ptr<udp_internal_multicast_transport>,
        std::shared_ptr<udp_external_multicast_transport>> handle;
};

class node_runtime : public std::enable_shared_from_this<node_runtime>
{
public:
    node_runtime();
    ~node_runtime();

    void start();
    void request_stop();
    void wait();

    io_reactor_ptr_t io_reactor;
    cv_reactor_ptr_t cv_reactor;
    std::shared_ptr<node> node_instance;

    std::string console_address = "127.0.0.1:5000";

    // Transport registry (console / INI). Returns error message or empty on success.
    std::string add_transport(const std::string& name,
                              const std::string& type,
                              const std::unordered_map<std::string, std::string>& fields);
    std::string modify_transport(const std::string& name,
                                 const std::unordered_map<std::string, std::string>& fields);
    std::string delete_transport(const std::string& name);
    std::vector<transport_entry_s> list_transports() const;

    std::atomic<bool> stop_requested{false};

    // Soft stop signal for /stop (main loop performs teardown).
    void signal_stop() { stop_requested.store(true); }

private:
    std::unordered_map<std::string, transport_entry_s> transports_;
    std::thread io_thread_;
    std::thread cv_thread_;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_NODE_RUNTIME_HPP
