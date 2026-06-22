#include <bfc_tunnel/security/x25519_dh.hpp>

#include <botan/system_rng.h>
#include <botan/x25519.h>

#include <stdexcept>

namespace bfc_tunnel
{

namespace
{

void require_key_size(const key_t& key, const char* name)
{
    if (key.size() != x25519_dh::k_key_bytes)
    {
        throw std::invalid_argument(std::string(name) + " must be " + std::to_string(x25519_dh::k_key_bytes) +
                                      " bytes");
    }
}

} // namespace

key_t x25519_dh::shared(const key_t& private_key, const key_t& peer_public_key) const
{
    require_key_size(private_key, "private key");
    require_key_size(peer_public_key, "peer public key");

    const Botan::X25519_PrivateKey priv(private_key);
    const auto                      secret = priv.agree(peer_public_key.data(), peer_public_key.size());
    return key_t(secret.begin(), secret.end());
}

void x25519_dh::generate_ephemeral(key_t& private_key, key_t& public_key) const
{
    const Botan::X25519_PrivateKey key(Botan::system_rng());

    const auto sk = key.raw_private_key_bits();
    const auto pk = key.raw_public_key_bits();
    private_key.assign(sk.begin(), sk.end());
    public_key.assign(pk.begin(), pk.end());
}

} // namespace bfc_tunnel
