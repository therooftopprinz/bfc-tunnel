#include <gtest/gtest.h>

#include <botan/ed25519.h>

#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/security/x25519_dh.hpp>
#include <bfc_tunnel/utils/msg_utils.hpp>

namespace
{

std::pair<bfc_tunnel::key_t, bfc_tunnel::key_t> make_ed25519_keypair()
{
    bfc_tunnel::key_t seed;
    bfc_tunnel::key_t public_key;
    bfc_tunnel::x25519_dh dh;
    dh.generate_ephemeral(seed, public_key);

    const Botan::Ed25519_PrivateKey priv = Botan::Ed25519_PrivateKey::from_seed(seed);
    const auto pub = priv.public_key_bits();
    return {seed, bfc_tunnel::key_t(pub.begin(), pub.end())};
}

} // namespace

TEST(msg1_btp_message, sign_and_verify_round_trip)
{
    const auto [private_key, public_key] = make_ed25519_keypair();

    cum::msg1 msg;
    msg.id                        = 1;
    msg.sec_ctx                   = 0;
    msg.integrity_algorithm       = bfc_tunnel::E_EA_HMAC_SHA2_256;
    msg.confidentiality_algorithm = bfc_tunnel::E_CA_AES256;
    msg.dh_key_type               = bfc_tunnel::E_DHKT_X25519;
    msg.ephemeral                 = public_key;
    msg.duration_s                = 120;
    msg.priority                  = 42;
    msg.signature                 = {};

    msg.signature = bfc_tunnel::sign_btp_message(msg, bfc_tunnel::E_DHKT_X25519, private_key);

    EXPECT_TRUE(bfc_tunnel::verify_btp_message(msg, bfc_tunnel::E_DHKT_X25519, public_key));
}

TEST(msg1_btp_message, verify_fails_when_payload_tampered)
{
    const auto [private_key, public_key] = make_ed25519_keypair();

    cum::msg1 msg;
    msg.id                        = 2;
    msg.sec_ctx                   = 1;
    msg.integrity_algorithm       = bfc_tunnel::E_EA_HMAC_SHA2_256;
    msg.confidentiality_algorithm = bfc_tunnel::E_CA_AES256;
    msg.dh_key_type               = bfc_tunnel::E_DHKT_X25519;
    msg.ephemeral                 = public_key;
    msg.duration_s                = 120;
    msg.priority                  = 7;
    msg.signature                 = {};

    msg.signature = bfc_tunnel::sign_btp_message(msg, bfc_tunnel::E_DHKT_X25519, private_key);
    msg.priority  = 8;

    EXPECT_FALSE(bfc_tunnel::verify_btp_message(msg, bfc_tunnel::E_DHKT_X25519, public_key));
}

TEST(msg2_btp_message, sign_and_verify_round_trip)
{
    const auto [private_key, public_key] = make_ed25519_keypair();

    cum::msg2 msg;
    msg.id        = 3;
    msg.status    = cum::status_e::OK;
    msg.ephemeral = public_key;
    msg.signature = {};

    msg.signature = bfc_tunnel::sign_btp_message(msg, bfc_tunnel::E_DHKT_X25519, private_key);

    EXPECT_TRUE(bfc_tunnel::verify_btp_message(msg, bfc_tunnel::E_DHKT_X25519, public_key));
}
