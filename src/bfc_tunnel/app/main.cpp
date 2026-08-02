#include <bfc/configuration_parser.hpp>
#include <bfc_tunnel/console/codec.hpp>
#include <bfc_tunnel/console/console_commands.hpp>
#include <bfc_tunnel/console/console_server.hpp>
#include <bfc_tunnel/node_runtime.hpp>
#include <bfc_tunnel/utils/logger.hpp>
#include <bfc_tunnel/utils/string_utils.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace
{

void apply_kv(bfc_tunnel::node_runtime& rt, const std::string& key, const std::string& value)
{
    using namespace bfc_tunnel;

    if (key == "console.address")
    {
        rt.console_address = value;
        return;
    }

    if (key == "security.supported_integrity_algorithms" ||
        key == "security.supported_confidentiality_algorithms" ||
        key == "security.supported_dhke_key_types")
    {
        console::command_t cmd;
        cmd.path = "/security/config";
        cmd.verb = "modify";
        if (key == "security.supported_integrity_algorithms")
        {
            cmd.args["supported_integrity_algorithms"] = value;
        }
        else if (key == "security.supported_confidentiality_algorithms")
        {
            cmd.args["supported_confidentiality_algorithms"] = value;
        }
        else
        {
            cmd.args["supported_dhke_key_types"] = value;
        }
        console::dispatch_command(rt, cmd);
        return;
    }

    if (key.rfind("node.network.", 0) == 0)
    {
        console::command_t cmd;
        cmd.path = "/network/config";
        cmd.verb = "modify";
        cmd.args[key.substr(std::string("node.network.").size())] = value;
        console::dispatch_command(rt, cmd);
        return;
    }
}

void load_ini(bfc_tunnel::node_runtime& rt, const std::string& path)
{
    bfc::configuration_parser parser;
    if (!parser.load(path))
    {
        std::cerr << "warning: could not load " << path << "\n";
        return;
    }
    for (const auto& [k, v] : parser)
    {
        apply_kv(rt, k, v);
    }

    // Transports from INI: transport.size + transport-N.*
    auto size_s = parser.arg("transport.size");
    if (size_s)
    {
        auto n = bfc_tunnel::utils::str_as<size_t>(*size_s);
        if (n)
        {
            for (size_t i = 0; i < *n; ++i)
            {
                const std::string prefix = "transport-" + std::to_string(i) + ".";
                auto name = parser.arg(prefix + "name");
                auto type = parser.arg(prefix + "type");
                if (!name || !type)
                {
                    continue;
                }
                std::unordered_map<std::string, std::string> fields;
                for (const auto& [k, v] : parser)
                {
                    if (k.rfind(prefix, 0) == 0)
                    {
                        auto field = k.substr(prefix.size());
                        if (field != "name" && field != "type")
                        {
                            fields[field] = v;
                        }
                    }
                }
                auto err = rt.add_transport(*name, *type, fields);
                if (!err.empty())
                {
                    std::cerr << "warning: transport " << *name << ": " << err << "\n";
                }
            }
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    using namespace bfc_tunnel;

    if (!g_logger)
    {
        g_logger = std::make_unique<logger>("/tmp/bfc_tunnel.log");
        g_logger->set_log_stdout(true);
    }

    auto runtime = std::make_shared<node_runtime>();
    std::string ini_path;
    std::vector<std::pair<std::string, std::string>> delayed_kvs;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a.rfind("--", 0) == 0)
        {
            auto eq = a.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }
            delayed_kvs.emplace_back(a.substr(2, eq - 2), a.substr(eq + 1));
        }
        else if (ini_path.empty())
        {
            ini_path = a;
        }
    }

    // Reactors must be running before any dispatch_sync (INI transports, node init).
    runtime->start();

    if (!ini_path.empty())
    {
        load_ini(*runtime, ini_path);
    }
    for (const auto& [k, v] : delayed_kvs)
    {
        apply_kv(*runtime, k, v);
    }

    auto console = std::make_shared<console::console_server>(runtime->io_reactor, runtime);
    if (!console->start(runtime->console_address))
    {
        std::cerr << "failed to start console on " << runtime->console_address << "\n";
        runtime->request_stop();
        runtime->wait();
        return 1;
    }

    std::cerr << "bfc_tunnel console listening on " << runtime->console_address << "\n";

    while (!runtime->stop_requested.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Give console a moment to flush /stop STATUS.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    console->stop();
    runtime->request_stop();
    runtime->wait();
    return 0;
}
