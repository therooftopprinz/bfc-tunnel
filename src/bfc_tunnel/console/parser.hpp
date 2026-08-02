#ifndef BFC_TUNNEL_CONSOLE_PARSER_HPP
#define BFC_TUNNEL_CONSOLE_PARSER_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bfc_tunnel
{
namespace console
{

struct filter_t
{
    bool include = true; // + include, - exclude
    std::string key;
    std::string value;
};

struct command_t
{
    std::string path;   // e.g. /security/private_key
    std::string verb;   // list|add|delete|modify|select
    std::unordered_map<std::string, std::string> args;
    std::vector<filter_t> filters;
    std::vector<std::string> bare; // tokens without '=', e.g. config key name on delete
};

inline std::vector<std::string> split_ws(std::string_view line)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : line)
    {
        if (c == ' ' || c == '\t' || c == '\r')
        {
            if (!cur.empty())
            {
                out.push_back(std::move(cur));
                cur.clear();
            }
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty())
    {
        out.push_back(std::move(cur));
    }
    return out;
}

inline std::optional<command_t> parse_command(std::string_view line)
{
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
    {
        line.remove_suffix(1);
    }
    auto tokens = split_ws(line);
    if (tokens.empty())
    {
        return std::nullopt;
    }

    command_t cmd;
    std::string first = tokens[0];
    if (first == "/stop")
    {
        cmd.path = "/stop";
        cmd.verb = "stop";
        return cmd;
    }

    // path may be /foo or /foo/bar; verb is next token
    if (tokens.size() < 2)
    {
        return std::nullopt;
    }

    cmd.path = tokens[0];
    cmd.verb = tokens[1];

    for (size_t i = 2; i < tokens.size(); ++i)
    {
        const auto& tok = tokens[i];
        if ((tok[0] == '+' || tok[0] == '-') && tok.find('=') != std::string::npos)
        {
            filter_t f;
            f.include = tok[0] == '+';
            auto eq = tok.find('=');
            f.key = tok.substr(1, eq - 1);
            f.value = tok.substr(eq + 1);
            cmd.filters.push_back(std::move(f));
            continue;
        }

        auto eq = tok.find('=');
        if (eq == std::string::npos)
        {
            cmd.bare.push_back(tok);
            continue;
        }
        cmd.args.emplace(tok.substr(0, eq), tok.substr(eq + 1));
    }
    return cmd;
}

inline bool row_matches(const std::unordered_map<std::string, std::string>& row,
                        const std::vector<filter_t>& filters)
{
    for (const auto& f : filters)
    {
        auto it = row.find(f.key);
        const std::string val = (it == row.end()) ? "" : it->second;
        const bool match = val == f.value;
        if (f.include && !match)
        {
            return false;
        }
        if (!f.include && match)
        {
            return false;
        }
    }
    return true;
}

} // namespace console
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_CONSOLE_PARSER_HPP
