#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include <bfc/buffer.hpp>
#include <bfc/sized_buffer.hpp>

#include <bfc_tunnel/protocol/btprotocol.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/security/key_utils.hpp>
#include <bfc_tunnel/utils/msg_utils.hpp>

namespace
{

using bfc_key_t = bfc_tunnel::key_t;

bfc::sized_buffer make_network_refresh_frame(uint8_t sec_ctx,
                                             uint8_t integrity_algorithm,
                                             const bfc_key_t& integrity_key,
                                             bool protect)
{
    bfc::sized_buffer pdu(1024);
    auto frame = bfc_tunnel::prepare_frame(pdu);
    frame.set_ttl(0);
    frame.set_frame_type(bfc_tunnel::E_FRAME_TYPE_NETWORK);
    frame.set_sec_ctx(sec_ctx);
    frame.set_mac_size(bfc_tunnel::integrity_mac_size(integrity_algorithm));
    frame.set_sn(0);
    frame.set_src(1);
    frame.set_dst(0xFFFFFFFF);
    frame.set_ts(12345);

    cum::network_key_refresh msg;
    msg.keys.push_back(cum::network_key{
        sec_ctx,
        7,
        999999,
        integrity_algorithm,
        bfc_key_t(32, 0x11),
        bfc_tunnel::E_CA_NONE,
        bfc_key_t{},
    });

    EXPECT_TRUE(bfc_tunnel::encode_payload(frame, msg));
    pdu.resize(frame.get_size());
    if (protect)
    {
        EXPECT_TRUE(bfc_tunnel::protect_frame_mac(frame, integrity_algorithm, integrity_key));
    }
    return pdu;
}

} // namespace

TEST(network_key_utils, derive_network_keys_is_deterministic)
{
    const bfc_key_t base{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                         0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                         0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                         0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};

    const auto first = bfc_tunnel::derive_network_keys(base, bfc_tunnel::E_EA_HMAC_SHA2_256, bfc_tunnel::E_CA_CHACHA20);
    const auto second = bfc_tunnel::derive_network_keys(base, bfc_tunnel::E_EA_HMAC_SHA2_256, bfc_tunnel::E_CA_CHACHA20);

    EXPECT_EQ(first.integrity_key, second.integrity_key);
    EXPECT_EQ(first.confidentiality_key, second.confidentiality_key);
    EXPECT_EQ(first.integrity_key.size(), 32u);
    EXPECT_EQ(first.confidentiality_key.size(), 32u);
    EXPECT_NE(first.integrity_key, first.confidentiality_key);
}

TEST(network_key_utils, derive_network_keys_differs_by_algorithm)
{
    const bfc_key_t base(32, 0x42);

    const auto sha256 = bfc_tunnel::derive_network_keys(base, bfc_tunnel::E_EA_HMAC_SHA2_256, bfc_tunnel::E_CA_CHACHA20);
    const auto none = bfc_tunnel::derive_network_keys(base, bfc_tunnel::E_EA_NONE, bfc_tunnel::E_CA_NONE);

    EXPECT_NE(sha256.integrity_key, none.integrity_key);
    EXPECT_TRUE(none.integrity_key.empty());
    EXPECT_TRUE(none.confidentiality_key.empty());
}

TEST(network_key_utils, protect_and_verify_frame_mac_round_trip)
{
    const bfc_key_t integrity_key(32, 0x5a);
    auto pdu = make_network_refresh_frame(3, bfc_tunnel::E_EA_HMAC_SHA2_256, integrity_key, true);

    const auto frame = bfc_tunnel::to_frame(pdu.data(), pdu.size());
    EXPECT_EQ(frame.get_frame_type(), bfc_tunnel::E_FRAME_TYPE_NETWORK);
    EXPECT_EQ(frame.get_sec_ctx(), 3u);
    EXPECT_EQ(frame.get_mac_size(), 32u);
    EXPECT_EQ(frame.get_payload_type(), bfc_tunnel::E_PAYLOAD_TYPE_NETWORK_KEY_REFRESH);
    EXPECT_TRUE(bfc_tunnel::verify_frame_mac(
        frame,
        bfc::const_buffer_view(pdu.data(), pdu.size()),
        bfc_tunnel::E_EA_HMAC_SHA2_256,
        integrity_key));
}

TEST(network_key_utils, verify_frame_mac_rejects_tamper_and_wrong_key)
{
    const bfc_key_t integrity_key(32, 0x5a);
    auto pdu = make_network_refresh_frame(3, bfc_tunnel::E_EA_HMAC_SHA2_256, integrity_key, true);

    pdu.data()[pdu.size() - 1] ^= std::byte{0x01};
    auto frame = bfc_tunnel::to_frame(pdu.data(), pdu.size());
    EXPECT_FALSE(bfc_tunnel::verify_frame_mac(
        frame,
        bfc::const_buffer_view(pdu.data(), pdu.size()),
        bfc_tunnel::E_EA_HMAC_SHA2_256,
        integrity_key));

    pdu = make_network_refresh_frame(3, bfc_tunnel::E_EA_HMAC_SHA2_256, integrity_key, true);
    frame = bfc_tunnel::to_frame(pdu.data(), pdu.size());
    const bfc_key_t wrong_key(32, 0xa5);
    EXPECT_FALSE(bfc_tunnel::verify_frame_mac(
        frame,
        bfc::const_buffer_view(pdu.data(), pdu.size()),
        bfc_tunnel::E_EA_HMAC_SHA2_256,
        wrong_key));
}

TEST(network_key_utils, protect_frame_mac_none_integrity)
{
    const bfc_key_t empty_key;
    auto pdu = make_network_refresh_frame(1, bfc_tunnel::E_EA_NONE, empty_key, true);
    const auto frame = bfc_tunnel::to_frame(pdu.data(), pdu.size());
    EXPECT_EQ(frame.get_mac_size(), 0u);
    EXPECT_TRUE(bfc_tunnel::verify_frame_mac(
        frame,
        bfc::const_buffer_view(pdu.data(), pdu.size()),
        bfc_tunnel::E_EA_NONE,
        empty_key));
}

TEST(network_key_utils, protect_and_accept_frame_round_trip_with_confidentiality)
{
    const bfc_key_t integrity_key(32, 0x5a);
    const bfc_key_t confidentiality_key(32, 0xc3);

    bfc::sized_buffer pdu(1024);
    auto frame = bfc_tunnel::prepare_frame(pdu);
    frame.set_ttl(0);
    frame.set_frame_type(bfc_tunnel::E_FRAME_TYPE_NETWORK);
    frame.set_sec_ctx(3);
    frame.set_mac_size(bfc_tunnel::integrity_mac_size(bfc_tunnel::E_EA_HMAC_SHA2_256));
    frame.set_sn(42);
    frame.set_src(7);
    frame.set_dst(0xFFFFFFFF);
    frame.set_ts(12345);

    cum::network_key_refresh msg;
    msg.keys.push_back(cum::network_key{
        3,
        7,
        999999,
        bfc_tunnel::E_EA_HMAC_SHA2_256,
        bfc_key_t(32, 0x11),
        bfc_tunnel::E_CA_CHACHA20,
        bfc_key_t(32, 0x22),
    });
    ASSERT_TRUE(bfc_tunnel::encode_payload(frame, msg));
    pdu.resize(frame.get_size());

    std::vector<std::byte> plaintext(frame.get_payload(), frame.get_payload() + frame.get_payload_size());
    ASSERT_TRUE(bfc_tunnel::protect_frame(
        frame,
        bfc_tunnel::E_EA_HMAC_SHA2_256,
        integrity_key,
        bfc_tunnel::E_CA_CHACHA20,
        confidentiality_key));
    EXPECT_NE(0, std::memcmp(frame.get_payload(), plaintext.data(), plaintext.size()));

    bfc_tunnel::frame_t rx(pdu.data(), pdu.size());
    ASSERT_TRUE(bfc_tunnel::accept_frame(
        rx,
        bfc_tunnel::E_EA_HMAC_SHA2_256,
        integrity_key,
        bfc_tunnel::E_CA_CHACHA20,
        confidentiality_key));
    EXPECT_EQ(0, std::memcmp(rx.get_payload(), plaintext.data(), plaintext.size()));
}

TEST(network_key_utils, accept_frame_rejects_tamper)
{
    const bfc_key_t integrity_key(32, 0x5a);
    const bfc_key_t confidentiality_key(32, 0xc3);

    bfc::sized_buffer pdu(1024);
    auto frame = bfc_tunnel::prepare_frame(pdu);
    frame.set_ttl(0);
    frame.set_frame_type(bfc_tunnel::E_FRAME_TYPE_NETWORK);
    frame.set_sec_ctx(3);
    frame.set_mac_size(bfc_tunnel::integrity_mac_size(bfc_tunnel::E_EA_HMAC_SHA2_256));
    frame.set_sn(1);
    frame.set_src(2);
    frame.set_dst(3);
    frame.set_ts(4);

    cum::network_key_refresh msg;
    msg.keys.push_back(cum::network_key{
        3, 1, 9, bfc_tunnel::E_EA_HMAC_SHA2_256, bfc_key_t(32, 0x11),
        bfc_tunnel::E_CA_AES256, bfc_key_t(32, 0x22)});
    ASSERT_TRUE(bfc_tunnel::encode_payload(frame, msg));
    pdu.resize(frame.get_size());
    ASSERT_TRUE(bfc_tunnel::protect_frame(
        frame,
        bfc_tunnel::E_EA_HMAC_SHA2_256,
        integrity_key,
        bfc_tunnel::E_CA_AES256,
        confidentiality_key));

    pdu.data()[pdu.size() - 1] ^= std::byte{0x01};
    bfc_tunnel::frame_t rx(pdu.data(), pdu.size());
    EXPECT_FALSE(bfc_tunnel::accept_frame(
        rx,
        bfc_tunnel::E_EA_HMAC_SHA2_256,
        integrity_key,
        bfc_tunnel::E_CA_AES256,
        confidentiality_key));
}

TEST(network_key_utils, protect_frame_none_algorithms_are_noop)
{
    bfc::sized_buffer pdu(1024);
    auto frame = bfc_tunnel::prepare_frame(pdu);
    frame.set_ttl(0);
    frame.set_frame_type(bfc_tunnel::E_FRAME_TYPE_NETWORK);
    frame.set_sec_ctx(1);
    frame.set_mac_size_units(0);
    frame.set_sn(1);
    frame.set_src(2);
    frame.set_dst(3);
    frame.set_ts(4);

    cum::beacon msg;
    msg.flags = 0;
    ASSERT_TRUE(bfc_tunnel::encode_payload(frame, msg));
    pdu.resize(frame.get_size());

    std::vector<std::byte> before(frame.get_payload(), frame.get_payload() + frame.get_payload_size());
    const bfc_key_t empty;
    ASSERT_TRUE(bfc_tunnel::protect_frame(frame, bfc_tunnel::E_EA_NONE, empty, bfc_tunnel::E_CA_NONE, empty));
    EXPECT_EQ(0, std::memcmp(frame.get_payload(), before.data(), before.size()));
    bfc_tunnel::frame_t rx(pdu.data(), pdu.size());
    ASSERT_TRUE(bfc_tunnel::accept_frame(rx, bfc_tunnel::E_EA_NONE, empty, bfc_tunnel::E_CA_NONE, empty));
}

TEST(network_key_messages, beacon_and_request_response_round_trip)
{
    std::array<std::byte, 512> buf{};
    cum::per_codec_ctx encode_ctx(buf.data(), buf.size());

    cum::beacon beacon_msg;
    beacon_msg.flags = 0;
    cum::encode_per(beacon_msg, encode_ctx);

    cum::per_codec_ctx decode_beacon_ctx(buf.data(), buf.size() - encode_ctx.size());
    cum::beacon decoded_beacon;
    cum::decode_per(decoded_beacon, decode_beacon_ctx);
    EXPECT_EQ(decoded_beacon.flags, beacon_msg.flags);

    encode_ctx = cum::per_codec_ctx(buf.data(), buf.size());
    cum::network_keys_request request;
    request.id = 7;
    cum::encode_per(request, encode_ctx);

    cum::per_codec_ctx decode_request_ctx(buf.data(), buf.size() - encode_ctx.size());
    cum::network_keys_request decoded_request;
    cum::decode_per(decoded_request, decode_request_ctx);
    EXPECT_EQ(decoded_request.id, 7u);

    encode_ctx = cum::per_codec_ctx(buf.data(), buf.size());
    cum::network_keys_response response;
    response.id = 7;
    response.current_page = 0;
    response.total_page = 1;
    response.keys.push_back(cum::network_key{
        1,
        99,
        123456789,
        bfc_tunnel::E_EA_HMAC_SHA2_256,
        bfc_key_t(32, 0x11),
        bfc_tunnel::E_CA_CHACHA20,
        bfc_key_t(32, 0x22),
    });
    cum::encode_per(response, encode_ctx);

    cum::per_codec_ctx decode_response_ctx(buf.data(), buf.size() - encode_ctx.size());
    cum::network_keys_response decoded_response;
    cum::decode_per(decoded_response, decode_response_ctx);
    EXPECT_EQ(decoded_response.id, 7u);
    EXPECT_EQ(decoded_response.total_page, 1u);
    ASSERT_EQ(decoded_response.keys.size(), 1u);
    EXPECT_EQ(decoded_response.keys[0].sec_ctx, 1u);
    EXPECT_EQ(decoded_response.keys[0].priority, 99u);
    EXPECT_EQ(decoded_response.keys[0].integrity_algorithm, bfc_tunnel::E_EA_HMAC_SHA2_256);
}

TEST(network_key_messages, query_and_security_information_round_trip)
{
    std::array<std::byte, 512> buf{};
    cum::per_codec_ctx encode_ctx(buf.data(), buf.size());

    cum::query_network_security query;
    cum::encode_per(query, encode_ctx);

    cum::per_codec_ctx decode_query_ctx(buf.data(), buf.size() - encode_ctx.size());
    cum::query_network_security decoded_query;
    cum::decode_per(decoded_query, decode_query_ctx);
    (void)decoded_query;

    encode_ctx = cum::per_codec_ctx(buf.data(), buf.size());
    cum::network_security_information info;
    info.informations.push_back(cum::network_key_information{1, 42, 999});
    cum::encode_per(info, encode_ctx);

    cum::per_codec_ctx decode_info_ctx(buf.data(), buf.size() - encode_ctx.size());
    cum::network_security_information decoded_info;
    cum::decode_per(decoded_info, decode_info_ctx);
    ASSERT_EQ(decoded_info.informations.size(), 1u);
    EXPECT_EQ(decoded_info.informations[0].sec_ctx, 1u);
    EXPECT_EQ(decoded_info.informations[0].priority, 42u);
}
