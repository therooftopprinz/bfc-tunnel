#include <gtest/gtest.h>

#include <bfc_tunnel/protocol/btprotocol.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/security/key_utils.hpp>

namespace
{

using bfc_key_t = bfc_tunnel::key_t;

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
