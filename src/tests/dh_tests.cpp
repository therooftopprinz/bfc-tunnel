#include <gtest/gtest.h>

#include <bfc_tunnel/security/dh.hpp>
#include <bfc_tunnel/security/dh_backend.hpp>
#include <bfc_tunnel/security/x25519_dh.hpp>

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
