#include <bfc_tunnel/node_runtime.hpp>

#include <bfc_tunnel/console/codec.hpp>
#include <bfc_tunnel/utils/logger.hpp>
#include <bfc_tunnel/utils/reactor_helper.hpp>
#include <bfc_tunnel/utils/string_utils.hpp>

#include <chrono>
#include <stdexcept>
#include <thread>

namespace bfc_tunnel
{

namespace
{

std::optional<uint16_t> field_u16(const std::unordered_map<std::string, std::string>& fields, const char* key)
{
    auto it = fields.find(key);
    if (it == fields.end())
    {
        return std::nullopt;
    }
    return utils::str_as<uint16_t>(it->second);
}

std::optional<size_t> field_size(const std::unordered_map<std::string, std::string>& fields, const char* key, size_t def)
{
    auto it = fields.find(key);
    if (it == fields.end())
    {
        return def;
    }
    auto v = utils::str_as<size_t>(it->second);
    return v ? *v : def;
}

} // namespace

node_runtime::node_runtime()
{
    io_reactor = std::make_shared<io_reactor_t>();
    cv_reactor = std::make_shared<cv_reactor_t>();
    node_instance = std::make_shared<node>(cv_reactor);
}

node_runtime::~node_runtime()
{
    request_stop();
    wait();
}

void node_runtime::start()
{
    cv_thread_ = std::thread([this]() { cv_reactor->run(); });
    io_thread_ = std::thread([this]() { io_reactor->run(); });
    // Allow reactor threads to enter run() before sync dispatches.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    node_instance->initialize();
}

void node_runtime::request_stop()
{
    if (stop_requested.exchange(true))
    {
        return;
    }
    for (auto& [name, entry] : transports_)
    {
        (void)name;
        if (entry.port)
        {
            node_instance->rem_port(entry.port);
        }
        std::visit([](auto& h) {
            if (h)
            {
                h->deinitialize();
            }
        }, entry.handle);
    }
    transports_.clear();
    node_instance->uninitialize();
    io_reactor->stop();
    cv_reactor->stop();
}

void node_runtime::wait()
{
    if (io_thread_.joinable())
    {
        io_thread_.join();
    }
    if (cv_thread_.joinable())
    {
        cv_thread_.join();
    }
}

std::vector<transport_entry_s> node_runtime::list_transports() const
{
    std::vector<transport_entry_s> out;
    out.reserve(transports_.size());
    for (const auto& [k, v] : transports_)
    {
        (void)k;
        out.push_back(v);
    }
    return out;
}

std::string node_runtime::add_transport(const std::string& name,
                                        const std::string& type,
                                        const std::unordered_map<std::string, std::string>& fields)
{
    if (name.empty())
    {
        return "name required";
    }
    if (transports_.count(name))
    {
        return "transport already exists";
    }

    auto queues = std::make_shared<transport_queue_pair_s>();
    auto port = std::make_shared<port_s>();
    port->name = name;
    port->mtu = field_size(fields, "mtu", 1500).value_or(1500);
    port->transport = queues;

    transport_entry_s entry;
    entry.name = name;
    entry.type = type;
    entry.fields = fields;
    entry.port = port;

    if (type == "UNICAST")
    {
        auto it = fields.find("bind");
        if (it == fields.end())
        {
            return "bind required for UNICAST";
        }
        auto addr = console::parse_sockaddr(it->second);
        if (!addr)
        {
            return "invalid bind address";
        }
        port->type = port_s::UNICAST;
        port->interface_address = *addr;

        auto tr = std::make_shared<udp_unicast_transport>(io_reactor, cv_reactor, queues);
        udp_unicast_transport_config_s cfg;
        cfg.address = it->second;
        tr->initialize(cfg);
        entry.handle = tr;
    }
    else if (type == "INTERNAL_MULTICAST")
    {
        auto git = fields.find("group");
        auto pit = field_u16(fields, "port");
        if (git == fields.end() || !pit)
        {
            return "group and port required for INTERNAL_MULTICAST";
        }
        std::string iface = "0.0.0.0";
        if (auto iit = fields.find("interface"); iit != fields.end())
        {
            iface = iit->second;
        }
        port->type = port_s::MULTICAST;
        auto tr = std::make_shared<udp_internal_multicast_transport>(io_reactor, cv_reactor, queues);
        udp_internal_multicast_transport_config_s cfg;
        cfg.group = git->second;
        cfg.port = *pit;
        cfg.interface = iface;
        tr->initialize(cfg);
        entry.handle = tr;
    }
    else if (type == "EXTERNAL_MULTICAST")
    {
        auto sit = fields.find("send");
        auto rit = fields.find("recv");
        if (sit == fields.end() || rit == fields.end())
        {
            return "send and recv required for EXTERNAL_MULTICAST";
        }
        port->type = port_s::MULTICAST;
        auto tr = std::make_shared<udp_external_multicast_transport>(io_reactor, cv_reactor, queues);
        udp_external_multicast_transport_config_s cfg;
        cfg.send = sit->second;
        cfg.recv = rit->second;
        tr->initialize(cfg);
        entry.handle = tr;
    }
    else
    {
        return "unknown transport type";
    }

    node_instance->add_port(port);
    transports_.emplace(name, std::move(entry));
    return {};
}

std::string node_runtime::modify_transport(const std::string& name,
                                           const std::unordered_map<std::string, std::string>& fields)
{
    auto it = transports_.find(name);
    if (it == transports_.end())
    {
        return "transport not found";
    }
    // Recreate: delete then add with merged fields.
    auto type = it->second.type;
    auto merged = it->second.fields;
    for (const auto& [k, v] : fields)
    {
        merged[k] = v;
    }
    auto err = delete_transport(name);
    if (!err.empty())
    {
        return err;
    }
    return add_transport(name, type, merged);
}

std::string node_runtime::delete_transport(const std::string& name)
{
    auto it = transports_.find(name);
    if (it == transports_.end())
    {
        return "transport not found";
    }
    if (it->second.port)
    {
        node_instance->rem_port(it->second.port);
    }
    std::visit([](auto& h) {
        if (h)
        {
            h->deinitialize();
        }
    }, it->second.handle);
    transports_.erase(it);
    return {};
}

} // namespace bfc_tunnel
