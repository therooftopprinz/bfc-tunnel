#include <bfc_tunnel/console/console_server.hpp>

#include <bfc_tunnel/console/codec.hpp>
#include <bfc_tunnel/console/console_commands.hpp>
#include <bfc_tunnel/console/parser.hpp>
#include <bfc_tunnel/console/table.hpp>
#include <bfc_tunnel/utils/logger.hpp>

#include <fcntl.h>
#include <unistd.h>

#include <cstring>

namespace bfc_tunnel
{
namespace console
{

namespace
{

void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
    {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

} // namespace

console_server::console_server(io_reactor_ptr_t io_reactor, std::shared_ptr<node_runtime> runtime)
    : io_reactor_(std::move(io_reactor))
    , runtime_(std::move(runtime))
{}

console_server::~console_server()
{
    stop();
}

bool console_server::start(const std::string& address)
{
    auto parsed = split_host_port(address);
    if (!parsed)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "console_server: invalid address %s", address.c_str());
        return false;
    }
    const auto& [host, port] = *parsed;

    listen_sock_ = bfc::socket(bfc::create_tcp4());
    if (listen_sock_.fd() < 0)
    {
        return false;
    }
    listen_sock_.set_sock_opt(SOL_SOCKET, SO_REUSEADDR, 1);
    set_nonblock(listen_sock_.fd());

    auto addr = bfc::ip4_port_to_sockaddr(host, port);
    if (addr.sin_family == 0 || listen_sock_.bind(addr) < 0 || listen_sock_.listen(16) < 0)
    {
        log(*g_logger, E_LOG_BIT_ERROR, "console_server: bind/listen failed on %s", address.c_str());
        listen_sock_ = {};
        return false;
    }

    io_reactor_->add_read_rdy(listen_sock_.fd(), [w = weak_from_this()]() {
        if (auto self = w.lock())
        {
            self->on_accept_ready();
        }
    });
    return true;
}

void console_server::stop()
{
    if (listen_sock_.fd() >= 0)
    {
        const int fd = listen_sock_.fd();
        io_reactor_->rem_read_rdy(fd);
        listen_sock_ = {};
    }
    std::vector<int> fds;
    for (auto& [fd, c] : clients_)
    {
        (void)c;
        fds.push_back(fd);
    }
    for (int fd : fds)
    {
        close_client(fd);
    }
}

void console_server::on_accept_ready()
{
    while (true)
    {
        sockaddr_in peer{};
        auto client_sock = listen_sock_.accept(peer);
        if (client_sock.fd() < 0)
        {
            break;
        }
        set_nonblock(client_sock.fd());
        const int fd = client_sock.fd();
        auto client = std::make_shared<client_s>();
        client->sock = std::move(client_sock);
        clients_[fd] = client;

        io_reactor_->add_read_rdy(fd, [w = weak_from_this(), fd]() {
            if (auto self = w.lock())
            {
                self->on_client_read(fd);
            }
        });
    }
}

void console_server::on_client_read(int fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return;
    }
    auto& client = *it->second;
    char buf[4096];
    while (true)
    {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0)
        {
            client.read_buf.append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n == 0)
        {
            close_client(fd);
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            break;
        }
        close_client(fd);
        return;
    }

    while (true)
    {
        auto pos = client.read_buf.find('\n');
        if (pos == std::string::npos)
        {
            break;
        }
        std::string line = client.read_buf.substr(0, pos);
        client.read_buf.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (!line.empty())
        {
            handle_line(fd, line);
        }
    }
}

void console_server::queue_write(int fd, std::string data)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return;
    }
    auto& client = *it->second;
    client.write_buf += std::move(data);
    if (!client.write_armed)
    {
        client.write_armed = true;
        io_reactor_->add_write_rdy(fd, [w = weak_from_this(), fd]() {
            if (auto self = w.lock())
            {
                self->on_client_write(fd);
            }
        });
    }
    io_reactor_->req_write(fd);
}

void console_server::on_client_write(int fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return;
    }
    auto& client = *it->second;
    while (!client.write_buf.empty())
    {
        ssize_t n = ::send(fd, client.write_buf.data(), client.write_buf.size(), MSG_NOSIGNAL);
        if (n > 0)
        {
            client.write_buf.erase(0, static_cast<size_t>(n));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            io_reactor_->req_write(fd);
            return;
        }
        close_client(fd);
        return;
    }
    if (client.write_armed && client.write_buf.empty())
    {
        // keep write interest registered but idle; next queue_write will req_write
    }
}

void console_server::close_client(int fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return;
    }
    io_reactor_->rem_read_rdy(fd);
    if (it->second->write_armed)
    {
        io_reactor_->rem_write_rdy(fd);
    }
    clients_.erase(it);
}

void console_server::handle_line(int fd, const std::string& line)
{
    auto cmd = parse_command(line);
    if (!cmd)
    {
        queue_write(fd, frame_reply(status_err("parse error")));
        return;
    }

    std::string body = dispatch_command(*runtime_, *cmd);
    queue_write(fd, frame_reply(body));

    if (cmd->path == "/stop")
    {
        runtime_->signal_stop();
    }
}

} // namespace console
} // namespace bfc_tunnel
