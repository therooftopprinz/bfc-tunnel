#ifndef BFC_TUNNEL_PROTOCOL_HPP
#define BFC_TUNNEL_PROTOCOL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <variant>

#include <bfc/buffer.hpp>

#include <bfc_tunnel/safeint.hpp>

namespace bfc_tunnel
{

enum message_type_e
{
    E_MSG_TYPE_ID_REQUEST     = 0x00,
    E_MSG_TYPE_ID_RESPONSE    = 0x01,
    E_MSG_TYPE_LINK_INFO      = 0x02,
    E_MSG_TYPE_LINK_REPORT    = 0x03,
    E_MSG_TYPE_ROUTE_ANNOUNCE = 0x04,
    E_MSG_TYPE_HUB_ANNOUNCE   = 0x05,
    E_MSG_TYPE_P2P_INDICATION = 0x06,
    E_MSG_TYPE_TUNNEL_DATA    = 0x07,
};

using node_id_t = std::array<uint8_t, 16>;

struct __attribute__((packed)) header_s
{
    uint8_t version;
    uint8_t type;
    bfc_tunnel::BEU16UA net_id;
    bfc_tunnel::BEU16UA ttl;
    bfc_tunnel::BEU16UA size;
    node_id_t src;
    node_id_t dst;
};

struct __attribute__((packed)) id_request_s
{
    BEU64UA id;
    uint8_t domain;
    uint8_t flags;
};

struct __attribute__((packed)) id_response_s
{
    BEU64UA id;
    uint8_t status;
    node_id_t node_id;
};

struct __attribute__((packed)) link_info_s
{
    BEU64UA sender_time;
    BEU64UA rcv_pkt;
    BEU64UA snt_pkt;
    BEU64UA rcv_byt;
    BEU64UA snt_byt;
};

struct __attribute__((packed)) link_report_s
{
    BEU64UA sender_time;
    BEU16UA rx_drop_pct;
};

struct __attribute__((packed)) route_announce_entry_s
{
    node_id_t origin;
    BEU64UA sn;
    uint8_t count;
};

struct __attribute__((packed)) route_announce_s
{
    BEU16UA asn;
    BEU16UA page;
    BEU16UA total;
    uint8_t flags;
    uint8_t count;

    using entry_type_t = route_announce_entry_s;
};

struct __attribute__((packed)) hub_announce_entry_s
{
    node_id_t node_id;
};

struct __attribute__((packed)) hub_announce_s
{
    node_id_t origin;
    BEU64UA sn;
    BEU16UA page;
    BEU16UA total;
    uint8_t flags;
    uint8_t count;

    using entry_type_t = hub_announce_entry_s;
};

struct __attribute__((packed)) p2p_indication_s
{
    node_id_t origin;
    node_id_t target;
    BEU32UA hostv4;
    BEU16UA port;
};

inline bool validate_header(const bfc::buffer_view& buffer)
{
    if (buffer.size() < sizeof(header_s))
    {
        return false;
    }

    const header_s& header = *reinterpret_cast<const header_s*>(buffer.data());
    if (static_cast<std::size_t>(header.size) + sizeof(header_s) > buffer.size())
    {
        return false;
    }

    if (header.version != 1)
    {
        return false;
    }

    if (header.type < E_MSG_TYPE_ID_REQUEST || header.type > E_MSG_TYPE_TUNNEL_DATA)
    {
        return false;
    }

    return true;
}

template<typename T>
inline bool validate_payload(const header_s& header, const T& payload)
{
    const uint16_t payload_size = header.size;

    if constexpr (std::is_same_v<T, route_announce_s> || std::is_same_v<T, hub_announce_s>)
    {
        constexpr std::size_t payload_static_size = sizeof(T);
        const std::size_t payload_dynamic_size =
            static_cast<std::size_t>(payload.count) * sizeof(typename T::entry_type_t);
        if (static_cast<std::size_t>(payload_size) != payload_static_size + payload_dynamic_size)
        {
            return false;
        }
    }
    else
    {
        if (sizeof(T) != static_cast<std::size_t>(payload_size))
        {
            return false;
        }
    }

    return true;
}

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_PROTOCOL_HPP