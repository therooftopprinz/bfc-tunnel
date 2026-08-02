#include <bfc_tunnel/console/console_commands.hpp>

#include <bfc_tunnel/console/codec.hpp>
#include <bfc_tunnel/console/table.hpp>
#include <bfc_tunnel/utils/reactor_helper.hpp>
#include <bfc_tunnel/utils/string_utils.hpp>

#include <algorithm>

namespace bfc_tunnel
{
namespace console
{

namespace
{

std::string arg_or(const command_t& cmd, const char* key)
{
    auto it = cmd.args.find(key);
    return it == cmd.args.end() ? "" : it->second;
}

std::vector<std::vector<std::string>> filter_rows(
    const std::vector<std::string>& cols,
    std::vector<std::vector<std::string>> rows,
    const std::vector<filter_t>& filters)
{
    if (filters.empty())
    {
        return rows;
    }
    std::vector<std::vector<std::string>> out;
    for (auto& row : rows)
    {
        std::unordered_map<std::string, std::string> map;
        for (size_t i = 0; i < cols.size() && i < row.size(); ++i)
        {
            map[cols[i]] = row[i];
        }
        if (row_matches(map, filters))
        {
            out.push_back(std::move(row));
        }
    }
    return out;
}

std::string list_kv(const std::vector<std::pair<std::string, std::string>>& kvs,
                    const std::vector<filter_t>& filters)
{
    const std::vector<std::string> cols = {"key", "value"};
    std::vector<std::vector<std::string>> rows;
    for (const auto& [k, v] : kvs)
    {
        rows.push_back({k, v});
    }
    rows = filter_rows(cols, std::move(rows), filters);
    return render_table(cols, rows);
}

std::string security_config_get(node& n, const std::string& key)
{
    if (key == "supported_integrity_algorithms")
    {
        std::vector<std::string> parts;
        for (auto a : n.get_supported_integrity_algorithms())
        {
            parts.push_back(format_integrity_algo(a));
        }
        return join_csv(parts);
    }
    if (key == "supported_confidentiality_algorithms")
    {
        std::vector<std::string> parts;
        for (auto a : n.get_supported_confidentiality_algorithms())
        {
            parts.push_back(format_conf_algo(a));
        }
        return join_csv(parts);
    }
    if (key == "supported_dhke_key_types")
    {
        std::vector<std::string> parts;
        for (auto a : n.get_supported_dhke_key_types())
        {
            parts.push_back(format_dhke(a));
        }
        return join_csv(parts);
    }
    return {};
}

bool security_config_set(node& n, const std::string& key, const std::string& value, std::string& err)
{
    if (key == "supported_integrity_algorithms")
    {
        u8_vec_t algs;
        for (const auto& p : split_csv(value))
        {
            auto a = parse_integrity_algo(p);
            if (!a)
            {
                err = "invalid integrity algorithm";
                return false;
            }
            algs.push_back(*a);
        }
        n.set_supported_integrity_algorithms(algs);
        return true;
    }
    if (key == "supported_confidentiality_algorithms")
    {
        u8_vec_t algs;
        for (const auto& p : split_csv(value))
        {
            auto a = parse_conf_algo(p);
            if (!a)
            {
                err = "invalid confidentiality algorithm";
                return false;
            }
            algs.push_back(*a);
        }
        n.set_supported_confidentiality_algorithms(algs);
        return true;
    }
    if (key == "supported_dhke_key_types")
    {
        std::vector<dh_key_type_e> types;
        for (const auto& p : split_csv(value))
        {
            auto a = parse_dhke(p);
            if (!a)
            {
                err = "invalid dhke key type";
                return false;
            }
            types.push_back(*a);
        }
        n.set_supported_dhke_key_types(types);
        return true;
    }
    err = "unknown security config key";
    return false;
}

void security_config_reset(node& n, const std::string& key)
{
    if (key == "supported_integrity_algorithms")
    {
        n.set_supported_integrity_algorithms({});
    }
    else if (key == "supported_confidentiality_algorithms")
    {
        n.set_supported_confidentiality_algorithms({});
    }
    else if (key == "supported_dhke_key_types")
    {
        n.set_supported_dhke_key_types({});
    }
}

std::string network_config_get(const node_config_s& c, const std::string& key)
{
    if (key == "beacon_interval_ms") return std::to_string(c.beacon_interval_ms);
    if (key == "check_peer_activity_interval_ms") return std::to_string(c.check_peer_activity_interval_ms);
    if (key == "default_peer_timeout_s") return std::to_string(c.default_peer_timeout_s);
    if (key == "security_ctx_timeout_s") return std::to_string(c.security_ctx_timeout_s);
    if (key == "security_ctx_grace_period_s") return std::to_string(c.security_ctx_grace_period_s);
    if (key == "transaction_timeout_ms") return std::to_string(c.transaction_timeout_ms);
    if (key == "key_refresh_interval_s") return std::to_string(c.key_refresh_interval_s);
    if (key == "security_query_timeout_ms") return std::to_string(c.security_query_timeout_ms);
    if (key == "query_network_security_retry_timeout_s") return std::to_string(c.query_network_security_retry_timeout_s);
    if (key == "network_key_seeder") return c.network_key_seeder ? "true" : "false";
    return {};
}

bool network_config_set(node_config_s& c, const std::string& key, const std::string& value, std::string& err)
{
    auto set_u64 = [&](uint64_t& dest) {
        auto v = utils::str_as<uint64_t>(value);
        if (!v)
        {
            err = "invalid integer";
            return false;
        }
        dest = *v;
        return true;
    };
    if (key == "beacon_interval_ms") return set_u64(c.beacon_interval_ms);
    if (key == "check_peer_activity_interval_ms") return set_u64(c.check_peer_activity_interval_ms);
    if (key == "default_peer_timeout_s") return set_u64(c.default_peer_timeout_s);
    if (key == "security_ctx_timeout_s") return set_u64(c.security_ctx_timeout_s);
    if (key == "security_ctx_grace_period_s") return set_u64(c.security_ctx_grace_period_s);
    if (key == "transaction_timeout_ms") return set_u64(c.transaction_timeout_ms);
    if (key == "key_refresh_interval_s") return set_u64(c.key_refresh_interval_s);
    if (key == "security_query_timeout_ms") return set_u64(c.security_query_timeout_ms);
    if (key == "query_network_security_retry_timeout_s") return set_u64(c.query_network_security_retry_timeout_s);
    if (key == "network_key_seeder")
    {
        if (value == "true" || value == "1")
        {
            c.network_key_seeder = true;
            return true;
        }
        if (value == "false" || value == "0")
        {
            c.network_key_seeder = false;
            return true;
        }
        err = "invalid bool";
        return false;
    }
    err = "unknown network config key";
    return false;
}

void network_config_reset(node_config_s& c, const std::string& key)
{
    node_config_s def;
    if (key == "beacon_interval_ms") c.beacon_interval_ms = def.beacon_interval_ms;
    else if (key == "check_peer_activity_interval_ms") c.check_peer_activity_interval_ms = def.check_peer_activity_interval_ms;
    else if (key == "default_peer_timeout_s") c.default_peer_timeout_s = def.default_peer_timeout_s;
    else if (key == "security_ctx_timeout_s") c.security_ctx_timeout_s = def.security_ctx_timeout_s;
    else if (key == "security_ctx_grace_period_s") c.security_ctx_grace_period_s = def.security_ctx_grace_period_s;
    else if (key == "transaction_timeout_ms") c.transaction_timeout_ms = def.transaction_timeout_ms;
    else if (key == "key_refresh_interval_s") c.key_refresh_interval_s = def.key_refresh_interval_s;
    else if (key == "security_query_timeout_ms") c.security_query_timeout_ms = def.security_query_timeout_ms;
    else if (key == "query_network_security_retry_timeout_s") c.query_network_security_retry_timeout_s = def.query_network_security_retry_timeout_s;
    else if (key == "network_key_seeder") c.network_key_seeder = def.network_key_seeder;
}

const std::vector<std::string> k_security_keys = {
    "supported_integrity_algorithms",
    "supported_confidentiality_algorithms",
    "supported_dhke_key_types",
};

const std::vector<std::string> k_network_keys = {
    "beacon_interval_ms",
    "check_peer_activity_interval_ms",
    "default_peer_timeout_s",
    "security_ctx_timeout_s",
    "security_ctx_grace_period_s",
    "transaction_timeout_ms",
    "key_refresh_interval_s",
    "security_query_timeout_ms",
    "query_network_security_retry_timeout_s",
    "network_key_seeder",
};

std::string handle_security_private_key(node_runtime& rt, const command_t& cmd)
{
    auto& n = *rt.node_instance;
    if (cmd.verb == "list")
    {
        std::vector<std::string> cols = {"node_id", "private_key"};
        std::vector<std::vector<std::string>> rows;
        utils::dispatch_sync(*rt.cv_reactor, [&]() {
            for (const auto& [id, key] : n.get_private_keys())
            {
                rows.push_back({format_node_id(id), base64_encode(key)});
            }
        });
        return render_table(cols, filter_rows(cols, std::move(rows), cmd.filters));
    }
    if (cmd.verb == "add" || cmd.verb == "modify")
    {
        auto id = parse_node_id(arg_or(cmd, "node_id"));
        auto key = base64_decode(arg_or(cmd, "private_key"));
        if (!id || !key)
        {
            return status_err("node_id and private_key required");
        }
        n.add_private_key(*id, *key);
        return render_table({"node_id", "private_key"}, {{format_node_id(*id), base64_encode(*key)}});
    }
    if (cmd.verb == "delete")
    {
        auto id = parse_node_id(arg_or(cmd, "node_id"));
        if (!id)
        {
            return status_err("node_id required");
        }
        n.rem_private_key(*id);
        return status_ok();
    }
    return status_err("unknown verb");
}

std::string handle_security_public_key(node_runtime& rt, const command_t& cmd)
{
    auto& n = *rt.node_instance;
    if (cmd.verb == "list")
    {
        std::vector<std::string> cols = {"node_id", "key_type", "public_key"};
        std::vector<std::vector<std::string>> rows;
        utils::dispatch_sync(*rt.cv_reactor, [&]() {
            for (const auto& [id, pk] : n.get_public_keys())
            {
                rows.push_back({format_node_id(id), format_dhke(pk.key_type), base64_encode(pk.public_key)});
            }
        });
        return render_table(cols, filter_rows(cols, std::move(rows), cmd.filters));
    }
    if (cmd.verb == "add" || cmd.verb == "modify")
    {
        auto id = parse_node_id(arg_or(cmd, "node_id"));
        auto key = base64_decode(arg_or(cmd, "public_key"));
        if (!id || !key)
        {
            return status_err("node_id and public_key required");
        }
        dh_key_type_e kt = E_DHKT_X25519;
        if (auto s = arg_or(cmd, "key_type"); !s.empty())
        {
            auto parsed = parse_dhke(s);
            if (!parsed)
            {
                return status_err("invalid key_type");
            }
            kt = *parsed;
        }
        peer_public_key_s pk{kt, *key};
        n.add_public_key(*id, pk);
        return render_table({"node_id", "key_type", "public_key"},
                            {{format_node_id(*id), format_dhke(kt), base64_encode(*key)}});
    }
    if (cmd.verb == "delete")
    {
        auto id = parse_node_id(arg_or(cmd, "node_id"));
        if (!id)
        {
            return status_err("node_id required");
        }
        n.rem_public_key(*id);
        return status_ok();
    }
    return status_err("unknown verb");
}

std::string handle_security_config(node_runtime& rt, const command_t& cmd)
{
    auto& n = *rt.node_instance;
    if (cmd.verb == "list")
    {
        std::vector<std::pair<std::string, std::string>> kvs;
        utils::dispatch_sync(*rt.cv_reactor, [&]() {
            for (const auto& k : k_security_keys)
            {
                kvs.emplace_back(k, security_config_get(n, k));
            }
        });
        return list_kv(kvs, cmd.filters);
    }

    // add/modify: take first name=value that is a known security key
    if (cmd.verb == "add" || cmd.verb == "modify")
    {
        for (const auto& k : k_security_keys)
        {
            auto it = cmd.args.find(k);
            if (it == cmd.args.end())
            {
                continue;
            }
            std::string err;
            if (!security_config_set(n, k, it->second, err))
            {
                return status_err(err);
            }
            std::string val;
            utils::dispatch_sync(*rt.cv_reactor, [&]() { val = security_config_get(n, k); });
            return render_table({"key", "value"}, {{k, val}});
        }
        return status_err("no security config key provided");
    }
    if (cmd.verb == "delete")
    {
        std::string key = cmd.bare.empty() ? "" : cmd.bare.front();
        if (key.empty())
        {
            for (const auto& k : k_security_keys)
            {
                if (cmd.args.count(k))
                {
                    key = k;
                    break;
                }
            }
        }
        if (std::find(k_security_keys.begin(), k_security_keys.end(), key) == k_security_keys.end())
        {
            return status_err("unknown security config key");
        }
        security_config_reset(n, key);
        std::string val;
        utils::dispatch_sync(*rt.cv_reactor, [&]() { val = security_config_get(n, key); });
        return render_table({"key", "value"}, {{key, val}});
    }
    return status_err("unknown verb");
}

std::string handle_transport(node_runtime& rt, const command_t& cmd)
{
    if (cmd.verb == "list")
    {
        auto entries = rt.list_transports();
        // Collect union of field keys for columns; keep name,type first.
        std::vector<std::string> cols = {"name", "type"};
        auto add_col = [&](const std::string& c) {
            if (std::find(cols.begin(), cols.end(), c) == cols.end())
            {
                cols.push_back(c);
            }
        };
        for (const auto& e : entries)
        {
            for (const auto& [k, v] : e.fields)
            {
                (void)v;
                add_col(k);
            }
        }
        std::vector<std::vector<std::string>> rows;
        for (const auto& e : entries)
        {
            std::vector<std::string> row;
            row.push_back(e.name);
            row.push_back(e.type);
            for (size_t i = 2; i < cols.size(); ++i)
            {
                auto it = e.fields.find(cols[i]);
                row.push_back(it == e.fields.end() ? "" : it->second);
            }
            rows.push_back(std::move(row));
        }
        return render_table(cols, filter_rows(cols, std::move(rows), cmd.filters));
    }
    if (cmd.verb == "add")
    {
        auto name = arg_or(cmd, "name");
        auto type = arg_or(cmd, "type");
        if (name.empty() || type.empty())
        {
            return status_err("name and type required");
        }
        auto fields = cmd.args;
        fields.erase("name");
        fields.erase("type");
        auto err = rt.add_transport(name, type, fields);
        if (!err.empty())
        {
            return status_err(err);
        }
        // echo row via list filter
        command_t list_cmd = cmd;
        list_cmd.verb = "list";
        list_cmd.filters = {{true, "name", name}};
        return handle_transport(rt, list_cmd);
    }
    if (cmd.verb == "modify")
    {
        auto name = arg_or(cmd, "name");
        if (name.empty())
        {
            return status_err("name required");
        }
        auto fields = cmd.args;
        fields.erase("name");
        auto err = rt.modify_transport(name, fields);
        if (!err.empty())
        {
            return status_err(err);
        }
        command_t list_cmd = cmd;
        list_cmd.verb = "list";
        list_cmd.filters = {{true, "name", name}};
        return handle_transport(rt, list_cmd);
    }
    if (cmd.verb == "delete")
    {
        auto name = arg_or(cmd, "name");
        if (name.empty())
        {
            return status_err("name required");
        }
        auto err = rt.delete_transport(name);
        if (!err.empty())
        {
            return status_err(err);
        }
        return status_ok();
    }
    return status_err("unknown verb");
}

std::string handle_network_identity(node_runtime& rt, const command_t& cmd)
{
    auto& n = *rt.node_instance;

    if (cmd.verb == "list")
    {
        std::vector<std::string> cols = {"node_id", "ds_send", "ds_recv"};
        std::vector<std::vector<std::string>> rows;
        utils::dispatch_sync(*rt.cv_reactor, [&]() {
            for (const auto& id : n.get_downstream_identities())
            {
                if (!id)
                {
                    continue;
                }
                rows.push_back({format_node_id(id->node_id),
                                format_sockaddr(id->downstream_address),
                                format_sockaddr(id->ds_recv_address)});
            }
        });
        return render_table(cols, filter_rows(cols, std::move(rows), cmd.filters));
    }
    if (cmd.verb == "add" || cmd.verb == "modify")
    {
        auto id = parse_node_id(arg_or(cmd, "node_id"));
        auto send = parse_sockaddr(arg_or(cmd, "ds_send"));
        auto recv = parse_sockaddr(arg_or(cmd, "ds_recv"));
        if (!id || !send || !recv)
        {
            return status_err("node_id, ds_send, ds_recv required");
        }
        auto identity = std::make_shared<downstream_identity_s>();
        identity->node_id = *id;
        identity->downstream_address = *send;
        identity->ds_recv_address = *recv;
        utils::dispatch_sync(*rt.cv_reactor, [&]() {
            auto pk = n.get_private_keys().find(*id);
            if (pk != n.get_private_keys().end())
            {
                identity->private_key = pk->second;
            }
        });
        n.add_downstream_identity(identity);
        return render_table({"node_id", "ds_send", "ds_recv"},
                            {{format_node_id(*id), format_sockaddr(*send), format_sockaddr(*recv)}});
    }
    if (cmd.verb == "delete")
    {
        auto id = parse_node_id(arg_or(cmd, "node_id"));
        if (!id)
        {
            return status_err("node_id required");
        }
        n.rem_downstream_identity(*id);
        return status_ok();
    }
    if (cmd.verb == "select")
    {
        auto id = parse_node_id(arg_or(cmd, "node_id"));
        if (!id)
        {
            return status_err("node_id required");
        }
        if (!n.select_downstream_identity(*id))
        {
            return status_err("identity not found");
        }
        std::string send;
        std::string recv;
        utils::dispatch_sync(*rt.cv_reactor, [&]() {
            auto sel = n.get_selected_downstream_identity();
            if (sel)
            {
                send = format_sockaddr(sel->downstream_address);
                recv = format_sockaddr(sel->ds_recv_address);
            }
        });
        return render_table({"node_id", "ds_send", "ds_recv"}, {{format_node_id(*id), send, recv}});
    }
    return status_err("unknown verb");
}

std::string handle_network_static_peer(node_runtime& rt, const command_t& cmd)
{
    auto& n = *rt.node_instance;
    if (cmd.verb == "list")
    {
        std::vector<std::string> cols = {"peer"};
        std::vector<std::vector<std::string>> rows;
        utils::dispatch_sync(*rt.cv_reactor, [&]() {
            for (const auto& p : n.get_static_peers())
            {
                rows.push_back({format_sockaddr(p)});
            }
        });
        return render_table(cols, filter_rows(cols, std::move(rows), cmd.filters));
    }
    if (cmd.verb == "add")
    {
        auto peer = parse_sockaddr(arg_or(cmd, "peer"));
        if (!peer)
        {
            return status_err("peer required");
        }
        n.add_static_peer(*peer);
        return render_table({"peer"}, {{format_sockaddr(*peer)}});
    }
    if (cmd.verb == "delete")
    {
        auto peer = parse_sockaddr(arg_or(cmd, "peer"));
        if (!peer)
        {
            return status_err("peer required");
        }
        n.rem_static_peer(*peer);
        return status_ok();
    }
    return status_err("unknown verb");
}

std::string handle_network_config(node_runtime& rt, const command_t& cmd)
{
    auto& n = *rt.node_instance;
    if (cmd.verb == "list")
    {
        std::vector<std::pair<std::string, std::string>> kvs;
        utils::dispatch_sync(*rt.cv_reactor, [&]() {
            const auto& c = n.get_node_config();
            for (const auto& k : k_network_keys)
            {
                kvs.emplace_back(k, network_config_get(c, k));
            }
        });
        return list_kv(kvs, cmd.filters);
    }
    if (cmd.verb == "add" || cmd.verb == "modify")
    {
        node_config_s c;
        utils::dispatch_sync(*rt.cv_reactor, [&]() { c = n.get_node_config(); });
        for (const auto& k : k_network_keys)
        {
            auto it = cmd.args.find(k);
            if (it == cmd.args.end())
            {
                continue;
            }
            std::string err;
            if (!network_config_set(c, k, it->second, err))
            {
                return status_err(err);
            }
            n.set_node_config(c);
            return render_table({"key", "value"}, {{k, network_config_get(c, k)}});
        }
        return status_err("no network config key provided");
    }
    if (cmd.verb == "delete")
    {
        std::string key = cmd.bare.empty() ? "" : cmd.bare.front();
        if (key.empty())
        {
            for (const auto& k : k_network_keys)
            {
                if (cmd.args.count(k))
                {
                    key = k;
                    break;
                }
            }
        }
        if (std::find(k_network_keys.begin(), k_network_keys.end(), key) == k_network_keys.end())
        {
            return status_err("unknown network config key");
        }
        node_config_s c;
        utils::dispatch_sync(*rt.cv_reactor, [&]() { c = n.get_node_config(); });
        network_config_reset(c, key);
        n.set_node_config(c);
        return render_table({"key", "value"}, {{key, network_config_get(c, key)}});
    }
    return status_err("unknown verb");
}

std::string handle_peer(node_runtime& rt, const command_t& cmd)
{
    if (cmd.verb != "list")
    {
        return status_err("only list supported");
    }
    auto& n = *rt.node_instance;
    std::vector<std::string> cols = {"node_id", "public_key", "preferred_transport", "preferred_address"};
    std::vector<std::vector<std::string>> rows;
    utils::dispatch_sync(*rt.cv_reactor, [&]() {
        for (const auto& [id, peer] : n.get_peers())
        {
            std::string pk;
            if (peer->public_key)
            {
                pk = base64_encode(peer->public_key->public_key);
            }
            else if (auto it = n.get_public_keys().find(id); it != n.get_public_keys().end())
            {
                pk = base64_encode(it->second.public_key);
            }
            std::string tr;
            std::string addr;
            if (peer->preferred_peer_address.port)
            {
                tr = peer->preferred_peer_address.port->name;
                addr = format_sockaddr(peer->preferred_peer_address.address);
            }
            rows.push_back({format_node_id(id), pk, tr, addr});
        }
    });
    return render_table(cols, filter_rows(cols, std::move(rows), cmd.filters));
}

std::string handle_peer_link(node_runtime& rt, const command_t& cmd)
{
    if (cmd.verb != "list")
    {
        return status_err("only list supported");
    }
    auto& n = *rt.node_instance;
    std::optional<node_id_t> only;
    if (auto s = arg_or(cmd, "node_id"); !s.empty())
    {
        only = parse_node_id(s);
        if (!only)
        {
            return status_err("invalid node_id");
        }
    }
    const bool include_id = !only.has_value();
    std::vector<std::string> cols;
    if (include_id)
    {
        cols.push_back("node_id");
    }
    cols.insert(cols.end(),
                {"transport_name", "last_activity", "sent_pkt", "recv_pkt", "sent_byt", "recv_byt"});

    std::vector<std::vector<std::string>> rows;
    utils::dispatch_sync(*rt.cv_reactor, [&]() {
        for (const auto& [id, peer] : n.get_peers())
        {
            if (only && *only != id)
            {
                continue;
            }
            auto append_links = [&](const peer_links_t& links) {
                for (const auto& [pa, ctx] : links)
                {
                    std::vector<std::string> row;
                    if (include_id)
                    {
                        row.push_back(format_node_id(id));
                    }
                    row.push_back(pa.port ? pa.port->name : "");
                    row.push_back(std::to_string(ctx.last_activity_time_s));
                    row.push_back(std::to_string(ctx.sent_pkt));
                    row.push_back(std::to_string(ctx.recv_pkt));
                    row.push_back(std::to_string(ctx.sent_byt));
                    row.push_back(std::to_string(ctx.recv_byt));
                    rows.push_back(std::move(row));
                }
            };
            append_links(peer->unicast_links);
            append_links(peer->multicast_links);
        }
    });
    return render_table(cols, filter_rows(cols, std::move(rows), cmd.filters));
}

} // namespace

std::string dispatch_command(node_runtime& runtime, const command_t& cmd)
{
    if (cmd.path == "/stop")
    {
        return status_ok();
    }
    if (cmd.path == "/security/private_key")
    {
        return handle_security_private_key(runtime, cmd);
    }
    if (cmd.path == "/security/public_key")
    {
        return handle_security_public_key(runtime, cmd);
    }
    if (cmd.path == "/security/config")
    {
        return handle_security_config(runtime, cmd);
    }
    if (cmd.path == "/transport")
    {
        return handle_transport(runtime, cmd);
    }
    if (cmd.path == "/network/identity")
    {
        return handle_network_identity(runtime, cmd);
    }
    if (cmd.path == "/network/static_peer")
    {
        return handle_network_static_peer(runtime, cmd);
    }
    if (cmd.path == "/network/config")
    {
        return handle_network_config(runtime, cmd);
    }
    if (cmd.path == "/peer")
    {
        return handle_peer(runtime, cmd);
    }
    if (cmd.path == "/peer/link")
    {
        return handle_peer_link(runtime, cmd);
    }
    return status_err("unknown command");
}

} // namespace console
} // namespace bfc_tunnel
