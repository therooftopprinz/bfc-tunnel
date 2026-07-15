#include <array>
#include <cstddef>

#include <gtest/gtest.h>

#include <bfc_tunnel/protocol/btprotocol.hpp>

namespace
{

template<typename T>
T round_trip(const T& msg)
{
    std::array<std::byte, 512> buf{};
    cum::per_codec_ctx encode_ctx(buf.data(), buf.size());
    cum::encode_per(msg, encode_ctx);

    const auto encoded_size = buf.size() - encode_ctx.size();
    T decoded{};
    cum::per_codec_ctx decode_ctx(buf.data(), encoded_size);
    cum::decode_per(decoded, decode_ctx);
    return decoded;
}

} // namespace

TEST(msg1_btp_message, encode_decode_round_trip)
{
    cum::msg1 msg;
    msg.id                        = 1;
    msg.sec_ctx                   = 0;
    msg.integrity_algorithm       = 1;
    msg.confidentiality_algorithm = 2;
    msg.dh_key_type               = 1;
    msg.ephemeral.assign({1, 2, 3, 4});
    msg.duration_s                = 120;
    msg.priority                  = 42;

    const auto decoded = round_trip(msg);
    EXPECT_EQ(decoded.id, msg.id);
    EXPECT_EQ(decoded.sec_ctx, msg.sec_ctx);
    EXPECT_EQ(decoded.integrity_algorithm, msg.integrity_algorithm);
    EXPECT_EQ(decoded.confidentiality_algorithm, msg.confidentiality_algorithm);
    EXPECT_EQ(decoded.dh_key_type, msg.dh_key_type);
    EXPECT_EQ(decoded.ephemeral, msg.ephemeral);
    EXPECT_EQ(decoded.duration_s, msg.duration_s);
    EXPECT_EQ(decoded.priority, msg.priority);
}

TEST(msg2_btp_message, encode_decode_round_trip)
{
    cum::msg2 msg;
    msg.id        = 3;
    msg.status    = cum::status_e::OK;
    msg.ephemeral.assign({5, 6, 7, 8});

    const auto decoded = round_trip(msg);
    EXPECT_EQ(decoded.id, msg.id);
    EXPECT_EQ(decoded.status, msg.status);
    EXPECT_EQ(decoded.ephemeral, msg.ephemeral);
}
