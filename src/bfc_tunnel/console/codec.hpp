#ifndef BFC_TUNNEL_CONSOLE_CODEC_HPP
#define BFC_TUNNEL_CONSOLE_CODEC_HPP

#include <bfc/socket.hpp>
#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/transport/transport_types.hpp>
#include <bfc_tunnel/utils/string_utils.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace bfc_tunnel
{
namespace console
{

inline std::optional<node_id_t> parse_node_id(const std::string& s)
{
    // Accept 0:0:0:1 or decimal
    if (s.find(':') != std::string::npos)
    {
        unsigned a = 0, b = 0, c = 0, d = 0;
        char sep1 = 0, sep2 = 0, sep3 = 0;
        std::istringstream iss(s);
        if (!(iss >> a >> sep1 >> b >> sep2 >> c >> sep3 >> d))
        {
            return std::nullopt;
        }
        if (sep1 != ':' || sep2 != ':' || sep3 != ':' || a > 255 || b > 255 || c > 255 || d > 255)
        {
            return std::nullopt;
        }
        return (node_id_t(a) << 24) | (node_id_t(b) << 16) | (node_id_t(c) << 8) | node_id_t(d);
    }
    return utils::str_as<node_id_t>(s);
}

inline std::string format_node_id(node_id_t id)
{
    std::ostringstream oss;
    oss << ((id >> 24) & 0xff) << ':'
        << ((id >> 16) & 0xff) << ':'
        << ((id >> 8) & 0xff) << ':'
        << (id & 0xff);
    return oss.str();
}

inline std::optional<std::pair<std::string, uint16_t>> split_host_port(const std::string& address)
{
    auto pos = address.find_last_of(':');
    if (pos == std::string::npos)
    {
        return std::nullopt;
    }
    auto port = utils::str_as<uint16_t>(address.substr(pos + 1));
    if (!port)
    {
        return std::nullopt;
    }
    return std::make_pair(address.substr(0, pos), *port);
}

inline std::optional<sockaddr_t> parse_sockaddr(const std::string& address)
{
    auto parsed = split_host_port(address);
    if (!parsed)
    {
        return std::nullopt;
    }
    const auto& [host, port] = *parsed;
    const bool v6 = std::count(host.begin(), host.end(), ':') > 0 || (!host.empty() && host[0] == '[');
    if (v6)
    {
        std::string h = host;
        if (!h.empty() && h.front() == '[' && h.back() == ']')
        {
            h = h.substr(1, h.size() - 2);
        }
        auto addr = bfc::ip6_port_to_sockaddr(h, port);
        if (addr.sin6_family == 0)
        {
            return std::nullopt;
        }
        return sockaddr_t{addr};
    }
    auto addr = bfc::ip4_port_to_sockaddr(host, port);
    if (addr.sin_family == 0)
    {
        return std::nullopt;
    }
    return sockaddr_t{addr};
}

inline std::string format_sockaddr(const sockaddr_t& addr)
{
    return std::visit(
        [](const auto& a) -> std::string {
            using T = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, sockaddr_none>)
            {
                return "";
            }
            else if constexpr (std::is_same_v<T, sockaddr_in>)
            {
                return bfc::sockaddr_to_string(const_cast<sockaddr*>(reinterpret_cast<const sockaddr*>(&a)));
            }
            else
            {
                return bfc::sockaddr_to_string(const_cast<sockaddr*>(reinterpret_cast<const sockaddr*>(&a)));
            }
        },
        addr);
}

// Minimal base64 (RFC 4648) for key material in console/CSV.
inline std::string base64_encode(const key_t& in)
{
    static constexpr char kTab[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 2 < in.size())
    {
        uint32_t n = (uint32_t(in[i]) << 16) | (uint32_t(in[i + 1]) << 8) | uint32_t(in[i + 2]);
        out.push_back(kTab[(n >> 18) & 63]);
        out.push_back(kTab[(n >> 12) & 63]);
        out.push_back(kTab[(n >> 6) & 63]);
        out.push_back(kTab[n & 63]);
        i += 3;
    }
    if (i < in.size())
    {
        uint32_t n = uint32_t(in[i]) << 16;
        out.push_back(kTab[(n >> 18) & 63]);
        if (i + 1 < in.size())
        {
            n |= uint32_t(in[i + 1]) << 8;
            out.push_back(kTab[(n >> 12) & 63]);
            out.push_back(kTab[(n >> 6) & 63]);
            out.push_back('=');
        }
        else
        {
            out.push_back(kTab[(n >> 12) & 63]);
            out.push_back('=');
            out.push_back('=');
        }
    }
    return out;
}

inline int b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

inline std::optional<key_t> base64_decode(const std::string& in)
{
    key_t out;
    out.reserve(in.size() * 3 / 4);
    int val = 0;
    int valb = -8;
    for (char c : in)
    {
        if (c == '=' || std::isspace(static_cast<unsigned char>(c)))
        {
            break;
        }
        int d = b64_val(c);
        if (d < 0)
        {
            return std::nullopt;
        }
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0)
        {
            out.push_back(uint8_t((val >> valb) & 0xff));
            valb -= 8;
        }
    }
    return out;
}

inline std::optional<uint8_t> parse_integrity_algo(const std::string& s)
{
    if (s == "NONE") return E_EA_NONE;
    if (s == "HMAC_SHA2_512") return E_EA_HMAC_SHA2_512;
    if (s == "HMAC_SHA2_256") return E_EA_HMAC_SHA2_256;
    if (s == "HMAC_BLAKE3") return E_EA_HMAC_BLAKE3;
    return std::nullopt;
}

inline std::string format_integrity_algo(uint8_t v)
{
    switch (v)
    {
        case E_EA_NONE: return "NONE";
        case E_EA_HMAC_SHA2_512: return "HMAC_SHA2_512";
        case E_EA_HMAC_SHA2_256: return "HMAC_SHA2_256";
        case E_EA_HMAC_BLAKE3: return "HMAC_BLAKE3";
        default: return std::to_string(v);
    }
}

inline std::optional<uint8_t> parse_conf_algo(const std::string& s)
{
    if (s == "NONE") return E_CA_NONE;
    if (s == "AES128") return E_CA_AES128;
    if (s == "AES256") return E_CA_AES256;
    if (s == "CHACHA20") return E_CA_CHACHA20;
    return std::nullopt;
}

inline std::string format_conf_algo(uint8_t v)
{
    switch (v)
    {
        case E_CA_NONE: return "NONE";
        case E_CA_AES128: return "AES128";
        case E_CA_AES256: return "AES256";
        case E_CA_CHACHA20: return "CHACHA20";
        default: return std::to_string(v);
    }
}

inline std::optional<dh_key_type_e> parse_dhke(const std::string& s)
{
    if (s == "NONE") return E_DHKT_NONE;
    if (s == "X25519") return E_DHKT_X25519;
    if (s == "SECP256R1") return E_DHKT_SECP256R1;
    if (s == "CURVE448") return E_DHKT_CURVE448;
    return std::nullopt;
}

inline std::string format_dhke(dh_key_type_e v)
{
    switch (v)
    {
        case E_DHKT_NONE: return "NONE";
        case E_DHKT_X25519: return "X25519";
        case E_DHKT_SECP256R1: return "SECP256R1";
        case E_DHKT_CURVE448: return "CURVE448";
        default: return std::to_string(static_cast<int>(v));
    }
}

inline std::vector<std::string> split_csv(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s)
    {
        if (c == ',')
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(c)))
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
    {
        out.push_back(cur);
    }
    return out;
}

inline std::string join_csv(const std::vector<std::string>& parts)
{
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i)
        {
            oss << ',';
        }
        oss << parts[i];
    }
    return oss.str();
}

} // namespace console
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_CONSOLE_CODEC_HPP
