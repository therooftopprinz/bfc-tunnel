#include <array>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include <bfc/buffer.hpp>

#include <bfc_tunnel/protocol.hpp>

namespace
{

using bfc::buffer_view;
using bfc_tunnel::E_MSG_TYPE_ID_REQUEST;
using bfc_tunnel::E_MSG_TYPE_TUNNEL_DATA;
using bfc_tunnel::header_s;
using bfc_tunnel::hub_announce_entry_s;
using bfc_tunnel::hub_announce_s;
using bfc_tunnel::id_request_s;
using bfc_tunnel::link_info_s;
using bfc_tunnel::route_announce_entry_s;
using bfc_tunnel::route_announce_s;
using bfc_tunnel::validate_header;
using bfc_tunnel::validate_payload;

header_s* header_at(std::array<std::byte, 256>& storage)
{
    return reinterpret_cast<header_s*>(storage.data());
}

buffer_view view_of(std::array<std::byte, 256>& storage, std::size_t used)
{
    return buffer_view(storage.data(), used);
}

} // namespace

TEST(protocol_validate_header, rejects_buffer_shorter_than_header)
{
    std::array<std::byte, 256> storage{};
    EXPECT_FALSE(validate_header(view_of(storage, sizeof(header_s) - 1)));
}

TEST(protocol_validate_header, rejects_when_declared_payload_exceeds_buffer)
{
    std::array<std::byte, 256> storage{};
    header_s* h = header_at(storage);
    h->version = 1;
    h->type = E_MSG_TYPE_ID_REQUEST;
    h->net_id = 0;
    h->ttl = 0;
    h->size = 1000;
    std::memset(h->src.data(), 0, h->src.size());
    std::memset(h->dst.data(), 0, h->dst.size());

    EXPECT_FALSE(validate_header(view_of(storage, sizeof(header_s))));
}

TEST(protocol_validate_header, rejects_wrong_version)
{
    std::array<std::byte, 256> storage{};
    header_s* h = header_at(storage);
    h->version = 2;
    h->type = E_MSG_TYPE_ID_REQUEST;
    h->net_id = 0;
    h->ttl = 0;
    h->size = static_cast<uint16_t>(sizeof(id_request_s));
    std::memset(h->src.data(), 0, h->src.size());
    std::memset(h->dst.data(), 0, h->dst.size());

    const std::size_t total = sizeof(header_s) + sizeof(id_request_s);
    EXPECT_FALSE(validate_header(view_of(storage, total)));
}

TEST(protocol_validate_header, rejects_type_above_range)
{
    std::array<std::byte, 256> storage{};
    header_s* h = header_at(storage);
    h->version = 1;
    h->type = static_cast<uint8_t>(E_MSG_TYPE_TUNNEL_DATA + 1);
    h->net_id = 0;
    h->ttl = 0;
    h->size = 0;
    std::memset(h->src.data(), 0, h->src.size());
    std::memset(h->dst.data(), 0, h->dst.size());

    EXPECT_FALSE(validate_header(view_of(storage, sizeof(header_s))));
}

TEST(protocol_validate_header, accepts_minimal_valid_header_with_zero_payload)
{
    std::array<std::byte, 256> storage{};
    header_s* h = header_at(storage);
    h->version = 1;
    h->type = E_MSG_TYPE_ID_REQUEST;
    h->net_id = 0;
    h->ttl = 0;
    h->size = 0;
    std::memset(h->src.data(), 0, h->src.size());
    std::memset(h->dst.data(), 0, h->dst.size());

    EXPECT_TRUE(validate_header(view_of(storage, sizeof(header_s))));
}

TEST(protocol_validate_header, accepts_header_plus_matching_payload_length)
{
    std::array<std::byte, 256> storage{};
    header_s* h = header_at(storage);
    h->version = 1;
    h->type = E_MSG_TYPE_ID_REQUEST;
    h->net_id = 1;
    h->ttl = 2;
    h->size = static_cast<uint16_t>(sizeof(id_request_s));
    std::memset(h->src.data(), 3, h->src.size());
    std::memset(h->dst.data(), 4, h->dst.size());

    const std::size_t total = sizeof(header_s) + sizeof(id_request_s);
    EXPECT_TRUE(validate_header(view_of(storage, total)));
}

TEST(protocol_validate_payload, fixed_payload_rejects_size_mismatch)
{
    header_s hdr{};
    hdr.size = static_cast<uint16_t>(sizeof(id_request_s) - 1);
    id_request_s payload{};
    EXPECT_FALSE(validate_payload(hdr, payload));
}

TEST(protocol_validate_payload, fixed_payload_accepts_matching_size)
{
    header_s hdr{};
    hdr.size = static_cast<uint16_t>(sizeof(link_info_s));
    link_info_s payload{};
    EXPECT_TRUE(validate_payload(hdr, payload));
}

TEST(protocol_validate_payload, route_announce_rejects_when_dynamic_length_wrong)
{
    route_announce_s ra{};
    ra.count = 2;
    header_s hdr{};
    hdr.size = static_cast<uint16_t>(sizeof(route_announce_s));
    EXPECT_FALSE(validate_payload(hdr, ra));
}

TEST(protocol_validate_payload, route_announce_accepts_zero_entries)
{
    route_announce_s ra{};
    ra.count = 0;
    header_s hdr{};
    hdr.size = static_cast<uint16_t>(sizeof(route_announce_s));
    EXPECT_TRUE(validate_payload(hdr, ra));
}

TEST(protocol_validate_payload, route_announce_accepts_one_entry)
{
    route_announce_s ra{};
    ra.count = 1;
    header_s hdr{};
    hdr.size = static_cast<uint16_t>(
        sizeof(route_announce_s) + sizeof(route_announce_entry_s));
    EXPECT_TRUE(validate_payload(hdr, ra));
}

TEST(protocol_validate_payload, hub_announce_accepts_two_entries)
{
    hub_announce_s ha{};
    ha.count = 2;
    header_s hdr{};
    hdr.size = static_cast<uint16_t>(
        sizeof(hub_announce_s) + 2 * sizeof(hub_announce_entry_s));
    EXPECT_TRUE(validate_payload(hdr, ha));
}
