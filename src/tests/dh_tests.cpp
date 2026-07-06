#include <gtest/gtest.h>

#include <bfc_tunnel/security/dh.hpp>
#include <bfc_tunnel/security/dh_backend.hpp>
#include <bfc_tunnel/security/x25519_dh.hpp>
#include <bfc_tunnel/protocol/frame.hpp>

namespace
{

class test_dh final : public bfc_tunnel::dh_backend
{
public:
    static constexpr size_t k_key_bytes = 4;

    size_t key_bytes() const override { return k_key_bytes; }

    bfc_tunnel::key_t shared(const bfc_tunnel::key_t& private_key,
                             const bfc_tunnel::key_t& peer_public_key) const override
    {
        bfc_tunnel::key_t out;
        out.reserve(private_key.size() + peer_public_key.size());
        out.insert(out.end(), private_key.begin(), private_key.end());
        out.insert(out.end(), peer_public_key.begin(), peer_public_key.end());
        return out;
    }

    void generate_ephemeral(bfc_tunnel::key_t& private_key, bfc_tunnel::key_t& public_key) const override
    {
        private_key = {0x01, 0x02, 0x03, 0x04};
        public_key  = {0x11, 0x12, 0x13, 0x14};
    }
};

} // namespace

TEST(dhke_kk, derives_noise_kk_shared_secrets_from_stored_keys)
{
    bfc_tunnel::dhke_kk alice(std::make_unique<test_dh>());
    alice.set_own_private({0xA1});
    alice.set_peer_public({0xB1});

    bfc_tunnel::key_t alice_ephemeral_public{};
    alice.get_own_ephemeral_public(alice_ephemeral_public);
    EXPECT_EQ(alice_ephemeral_public, (bfc_tunnel::key_t{0x11, 0x12, 0x13, 0x14}));

    bfc_tunnel::dhke_kk bob(std::make_unique<test_dh>());
    bob.set_own_private({0xB2});
    bob.set_peer_public({0xA2});
    bob.set_peer_ephemeral_public(alice_ephemeral_public);

    bfc_tunnel::key_t bob_ephemeral_public{};
    bob.get_own_ephemeral_public(bob_ephemeral_public);

    alice.set_peer_ephemeral_public(bob_ephemeral_public);

    EXPECT_EQ(alice.dh_eb(), (bfc_tunnel::key_t{0x01, 0x02, 0x03, 0x04, 0xB1}));
    EXPECT_EQ(alice.dh_ab(), (bfc_tunnel::key_t{0xA1, 0xB1}));
    EXPECT_EQ(alice.dh_ef(), (bfc_tunnel::key_t{0x01, 0x02, 0x03, 0x04, 0x11, 0x12, 0x13, 0x14}));
    EXPECT_EQ(alice.dh_af(), (bfc_tunnel::key_t{0xA1, 0x11, 0x12, 0x13, 0x14}));

    EXPECT_EQ(bob.dh_eb(), (bfc_tunnel::key_t{0x01, 0x02, 0x03, 0x04, 0xA2}));
    EXPECT_EQ(bob.dh_ab(), (bfc_tunnel::key_t{0xB2, 0xA2}));
    EXPECT_EQ(bob.dh_ef(), (bfc_tunnel::key_t{0x01, 0x02, 0x03, 0x04, 0x11, 0x12, 0x13, 0x14}));
    EXPECT_EQ(bob.dh_af(), (bfc_tunnel::key_t{0xB2, 0x11, 0x12, 0x13, 0x14}));
}

TEST(dhke_kk, factory_selects_x25519_backend)
{
    auto alice = bfc_tunnel::make_dhke_kk(bfc_tunnel::E_DHKT_X25519);
    auto bob   = bfc_tunnel::make_dhke_kk(bfc_tunnel::E_DHKT_X25519);

    bfc_tunnel::key_t alice_ephemeral_public{};
    alice.get_own_ephemeral_public(alice_ephemeral_public);
    EXPECT_EQ(alice_ephemeral_public.size(), bfc_tunnel::x25519_dh::k_key_bytes);

    bfc_tunnel::key_t bob_ephemeral_public{};
    bob.get_own_ephemeral_public(bob_ephemeral_public);
    EXPECT_EQ(bob_ephemeral_public.size(), bfc_tunnel::x25519_dh::k_key_bytes);

    alice.set_peer_ephemeral_public(bob_ephemeral_public);
    bob.set_peer_ephemeral_public(alice_ephemeral_public);

    const bfc_tunnel::key_t alice_ef = alice.dh_ef();
    const bfc_tunnel::key_t bob_ef   = bob.dh_ef();
    EXPECT_EQ(alice_ef.size(), bfc_tunnel::x25519_dh::k_key_bytes);
    EXPECT_EQ(bob_ef, alice_ef);
}

TEST(dhke_kk, rejects_unsupported_key_type)
{
    EXPECT_THROW(bfc_tunnel::make_dhke_kk(bfc_tunnel::E_DHKT_SECP256R1), std::invalid_argument);
}

TEST(dhke_kk, derives_matching_peer_keys_from_noise_kk_schedule)
{
    const bfc_tunnel::key_t alice_static_private{
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
        0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
    const bfc_tunnel::key_t bob_static_private{
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
        0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
        0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60};

    const auto alice_static_public = bfc_tunnel::x25519_dh::public_from_private(alice_static_private);
    const auto bob_static_public   = bfc_tunnel::x25519_dh::public_from_private(bob_static_private);

    auto alice = bfc_tunnel::make_dhke_kk(bfc_tunnel::E_DHKT_X25519);
    auto bob   = bfc_tunnel::make_dhke_kk(bfc_tunnel::E_DHKT_X25519);

    bfc_tunnel::key_t alice_ephemeral_public{};
    bfc_tunnel::key_t bob_ephemeral_public{};

    alice.setup_handshake(alice_static_private, bob_static_public, true);
    bob.setup_handshake(bob_static_private, alice_static_public, false);

    alice.get_own_ephemeral_public(alice_ephemeral_public);
    bob.get_own_ephemeral_public(bob_ephemeral_public);

    alice.set_peer_ephemeral_public(bob_ephemeral_public);
    bob.set_peer_ephemeral_public(alice_ephemeral_public);

    const auto alice_secrets = alice.handshake_secrets();
    const auto bob_secrets   = bob.handshake_secrets();
    EXPECT_EQ(alice_secrets.eb, bob_secrets.eb);
    EXPECT_EQ(alice_secrets.ab, bob_secrets.ab);
    EXPECT_EQ(alice_secrets.ef, bob_secrets.ef);
    EXPECT_EQ(alice_secrets.af, bob_secrets.af);

    const auto alice_integrity = alice.derive_integrity_key(bfc_tunnel::E_EA_HMAC_SHA2_256);
    const auto bob_integrity   = bob.derive_integrity_key(bfc_tunnel::E_EA_HMAC_SHA2_256);
    EXPECT_NE(alice_integrity, bob_integrity);
    EXPECT_EQ(alice_integrity, bob.derive_receive_integrity_key(bfc_tunnel::E_EA_HMAC_SHA2_256));
    EXPECT_EQ(bob_integrity, alice.derive_receive_integrity_key(bfc_tunnel::E_EA_HMAC_SHA2_256));
    EXPECT_EQ(alice_integrity.size(), 32U);

    const auto alice_confidentiality = alice.derive_confidentiality_key(bfc_tunnel::E_CA_AES256);
    const auto bob_confidentiality   = bob.derive_confidentiality_key(bfc_tunnel::E_CA_AES256);
    EXPECT_NE(alice_confidentiality, bob_confidentiality);
    EXPECT_EQ(alice_confidentiality, bob.derive_receive_confidentiality_key(bfc_tunnel::E_CA_AES256));
    EXPECT_EQ(bob_confidentiality, alice.derive_receive_confidentiality_key(bfc_tunnel::E_CA_AES256));
    EXPECT_EQ(alice_confidentiality.size(), 32U);

    EXPECT_TRUE(alice.derive_integrity_key(bfc_tunnel::E_EA_NONE).empty());
    EXPECT_TRUE(alice.derive_confidentiality_key(bfc_tunnel::E_CA_NONE).empty());
}
