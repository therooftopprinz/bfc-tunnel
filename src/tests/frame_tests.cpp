#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include <bfc/buffer.hpp>

#include <bfc_tunnel/protocol/frame.hpp>

namespace
{

using bfc::buffer_view;
using bfc_tunnel::E_FRAME_TYPE_NETWORK;
using bfc_tunnel::E_FRAME_TYPE_PUBLIC;
using bfc_tunnel::E_PAYLOAD_TYPE_BEACON;
using bfc_tunnel::frame_const_t;
using bfc_tunnel::frame_t;
using bfc_tunnel::k_frame_protocol_version;
using bfc_tunnel::validate_frame;
using bfc_tunnel::validate_frame_buffer;
using bfc_tunnel::frame_payload_view;

buffer_view view_of(std::array<std::byte, 256>& storage, std::size_t used)
{
    return buffer_view(storage.data(), used);
}

} // namespace

TEST(frame_header, stores_ttl_in_first_byte)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), storage.size());
    frame.set_ttl(8);

    EXPECT_EQ(frame.get_ttl(), 8);
    EXPECT_EQ(static_cast<uint8_t>(storage[0]), 0x08);
}

TEST(frame_header, packs_version_and_frame_type_in_second_byte)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), storage.size());
    frame.set_version(5);
    frame.set_frame_type(E_FRAME_TYPE_NETWORK);

    EXPECT_EQ(frame.get_version(), 5);
    EXPECT_EQ(frame.get_frame_type(), E_FRAME_TYPE_NETWORK);
    EXPECT_EQ(static_cast<uint8_t>(storage[1]), 0b00010101);
}

TEST(frame_header, packs_int_and_conf_algo_in_fourth_byte)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), storage.size());
    frame.set_int_algo(0xA);
    frame.set_conf_algo(0x5);

    EXPECT_EQ(frame.get_int_algo(), 0xA);
    EXPECT_EQ(frame.get_conf_algo(), 0x5);
    EXPECT_EQ(static_cast<uint8_t>(storage[3]), 0xA5);
}

TEST(frame_header, stores_u32_fields_big_endian_after_mac)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), storage.size());
    frame.set_sn(0x01020304);
    frame.set_ts(0x05060708);
    frame.set_src(0x090A0B0C);
    frame.set_dst(0x0D0E0F10);

    EXPECT_EQ(frame.get_sn(), 0x01020304u);
    EXPECT_EQ(frame.get_ts(), 0x05060708u);
    EXPECT_EQ(frame.get_src(), 0x090A0B0Cu);
    EXPECT_EQ(frame.get_dst(), 0x0D0E0F10u);

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(storage.data());
    EXPECT_EQ(raw[4], 0x01);
    EXPECT_EQ(raw[8], 0x05);
    EXPECT_EQ(raw[12], 0x09);
    EXPECT_EQ(raw[16], 0x0D);
}

TEST(frame_header, shifts_u32_fields_after_mac)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), storage.size(), 4);
    frame.set_sn(0x01020304);

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(storage.data());
    EXPECT_EQ(raw[8], 0x01);
    EXPECT_EQ(frame.get_mac() - raw, 4);
}

TEST(frame_header, reports_payload_size_after_fixed_header)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), 32);
    EXPECT_EQ(frame.get_header_size(), frame_const_t::k_fixed_header_size + frame_const_t::k_payload_type_size);
    EXPECT_EQ(frame.get_payload_size(), 32 - frame.get_header_size());
}

TEST(frame_header, stores_payload_type_before_payload)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), storage.size());
    frame.set_payload_type(E_PAYLOAD_TYPE_BEACON);

    EXPECT_EQ(frame.get_payload_type(), E_PAYLOAD_TYPE_BEACON);
    EXPECT_EQ(static_cast<uint8_t>(storage[frame_const_t::k_fixed_header_size]), 0x00);
}

TEST(frame_validate, rejects_buffer_shorter_than_header)
{
    std::array<std::byte, 256> storage{};
    frame_const_t frame(
        reinterpret_cast<const uint8_t*>(storage.data()),
        frame_const_t::k_fixed_header_size - 1
    );
    EXPECT_FALSE(validate_frame(frame));
    EXPECT_FALSE(validate_frame_buffer(view_of(storage, frame_const_t::k_fixed_header_size - 1)));
}

TEST(frame_validate, rejects_wrong_version)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), storage.size());
    frame.set_version(2);
    frame.set_frame_type(E_FRAME_TYPE_PUBLIC);

    frame_const_t cframe(
        reinterpret_cast<const uint8_t*>(storage.data()),
        storage.size()
    );
    EXPECT_FALSE(validate_frame(cframe));
}

TEST(frame_validate, rejects_mac_size_mismatch)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), storage.size());
    frame.set_version(k_frame_protocol_version);
    frame.set_frame_type(E_FRAME_TYPE_NETWORK);
    frame.set_sec_ctx(1);
    frame.set_int_algo(0);
    frame.set_mac_size(4);

    frame_const_t cframe(
        reinterpret_cast<const uint8_t*>(storage.data()),
        storage.size(),
        4
    );
    EXPECT_FALSE(validate_frame(cframe));
}

TEST(frame_validate, accepts_minimal_valid_frame)
{
    std::array<std::byte, 256> storage{};
    frame_t frame(reinterpret_cast<uint8_t*>(storage.data()), storage.size());
    frame.set_version(k_frame_protocol_version);
    frame.set_frame_type(E_FRAME_TYPE_NETWORK);
    frame.set_sec_ctx(0);
    frame.set_ttl(8);
    frame.set_src(42);
    frame.set_dst(99);

    frame_const_t cframe(
        reinterpret_cast<const uint8_t*>(storage.data()),
        storage.size()
    );
    EXPECT_TRUE(validate_frame(cframe));
    EXPECT_TRUE(validate_frame_buffer(view_of(storage, frame.get_header_size())));
}

TEST(frame_payload_view, returns_bytes_after_header)
{
    std::array<std::byte, 256> storage{};
    auto* raw = reinterpret_cast<uint8_t*>(storage.data());
    frame_t frame(raw, storage.size());
    frame.set_version(k_frame_protocol_version);
    frame.set_frame_type(E_FRAME_TYPE_NETWORK);
    frame.set_payload_type(E_PAYLOAD_TYPE_BEACON);

    raw[frame.get_header_size()] = 0xAB;
    raw[frame.get_header_size() + 1] = 0xCD;

    frame_const_t cframe(raw, storage.size());
    const auto payload = frame_payload_view(cframe, view_of(storage, frame.get_header_size() + 2));
    ASSERT_EQ(payload.size(), 2u);
    EXPECT_EQ(static_cast<uint8_t>(payload.data()[0]), 0xAB);
    EXPECT_EQ(static_cast<uint8_t>(payload.data()[1]), 0xCD);
}
