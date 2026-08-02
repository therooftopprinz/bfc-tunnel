#ifndef BFC_TUNNEL_CONSOLE_SERVER_HPP
#define BFC_TUNNEL_CONSOLE_SERVER_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/node_runtime.hpp>

#include <bfc/socket.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace bfc_tunnel
{
namespace console
{

class console_server : public std::enable_shared_from_this<console_server>
{
public:
    console_server(io_reactor_ptr_t io_reactor, std::shared_ptr<node_runtime> runtime);
    ~console_server();

    // address: host:port
    bool start(const std::string& address);
    void stop();

private:
    struct client_s
    {
        bfc::socket sock;
        std::string read_buf;
        std::string write_buf;
        bool write_armed = false;
    };

    void on_accept_ready();
    void on_client_read(int fd);
    void on_client_write(int fd);
    void close_client(int fd);
    void queue_write(int fd, std::string data);
    void handle_line(int fd, const std::string& line);

    io_reactor_ptr_t io_reactor_;
    std::shared_ptr<node_runtime> runtime_;
    bfc::socket listen_sock_;
    std::unordered_map<int, std::shared_ptr<client_s>> clients_;
};

} // namespace console
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_CONSOLE_SERVER_HPP
