#include <bfc_tunnel/security/key_utils.hpp>

#include <bfc_tunnel/security/x25519_dh.hpp>

#include <botan/ed25519.h>
#include <botan/pubkey.h>
#include <botan/system_rng.h>

#include <stdexcept>

namespace bfc_tunnel
{

key_t sign_x25519(const key_t& private_key, bfc::const_buffer_view message)
{
    if (private_key.size() != x25519_dh::k_key_bytes)
    {
        throw std::invalid_argument("private key must be " + std::to_string(x25519_dh::k_key_bytes) + " bytes");
    }

    const Botan::Ed25519_PrivateKey priv = Botan::Ed25519_PrivateKey::from_seed(private_key);
    Botan::PK_Signer                  signer(priv, Botan::system_rng(), "");
    const auto                        sig =
        signer.sign_message(reinterpret_cast<const uint8_t*>(message.data()), message.size(), Botan::system_rng());
    return key_t(sig.begin(), sig.end());
}

bool verify_x25519(const key_t& public_key, bfc::const_buffer_view message, const key_t& signature)
{
    if (public_key.size() != x25519_dh::k_key_bytes)
    {
        throw std::invalid_argument("public key must be " + std::to_string(x25519_dh::k_key_bytes) + " bytes");
    }

    const Botan::Ed25519_PublicKey pub(public_key);
    Botan::PK_Verifier             verifier(pub, "");
    return verifier.verify_message(reinterpret_cast<const uint8_t*>(message.data()),
                                   message.size(),
                                   signature.data(),
                                   signature.size());
}

key_t sign(dh_key_type_e key_type, const key_t& private_key, bfc::const_buffer_view message)
{
    switch (key_type)
    {
        case E_DHKT_X25519:
            return sign_x25519(private_key, message);
        case E_DHKT_NONE:
        case E_DHKT_SECP256R1:
        case E_DHKT_CURVE448:
        case E_DHKT_MAX:
            break;
    }

    return {};
}

bool verify(dh_key_type_e key_type, const key_t& public_key, bfc::const_buffer_view message, const key_t& signature)
{
    switch (key_type)
    {
        case E_DHKT_X25519:
            return verify_x25519(public_key, message, signature);
        case E_DHKT_NONE:
        case E_DHKT_SECP256R1:
        case E_DHKT_CURVE448:
        case E_DHKT_MAX:
            break;
    }

    return false;
}

} // namespace bfc_tunnel
