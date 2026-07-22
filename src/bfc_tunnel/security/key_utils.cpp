#include <bfc_tunnel/security/key_utils.hpp>

#include <bfc_tunnel/security/x25519_dh.hpp>

#include <botan/ed25519.h>
#include <botan/mac.h>
#include <botan/pubkey.h>
#include <botan/system_rng.h>

#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace bfc_tunnel
{

namespace
{

constexpr uint8_t k_label_integrity       = 0x01;
constexpr uint8_t k_label_confidentiality = 0x02;

key_t hmac_sha256(const key_t& key, bfc::const_buffer_view data)
{
    auto mac = Botan::MessageAuthenticationCode::create_or_throw("HMAC(SHA-256)");
    mac->set_key(key);
    mac->update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    const auto out = mac->final();
    return key_t(out.begin(), out.end());
}

std::pair<key_t, key_t> noise_hkdf2(const key_t& chaining_key, bfc::const_buffer_view input_key_material)
{
    const key_t temp_key = hmac_sha256(chaining_key, input_key_material);

    const std::array<uint8_t, 1> output1_input{0x01};
    const key_t                  output1 = hmac_sha256(temp_key, bfc::const_buffer_view(
        reinterpret_cast<const std::byte*>(output1_input.data()), output1_input.size()));

    key_t output2_input;
    output2_input.reserve(output1.size() + 1);
    output2_input.insert(output2_input.end(), output1.begin(), output1.end());
    output2_input.push_back(0x02);

    const key_t output2 = hmac_sha256(temp_key, bfc::const_buffer_view(
        reinterpret_cast<const std::byte*>(output2_input.data()), output2_input.size()));
    return {output1, output2};
}

key_t truncate_key(key_t key, size_t size)
{
    if (key.size() < size)
    {
        throw std::invalid_argument("derived key is shorter than requested size");
    }

    key.resize(size);
    return key;
}

key_t derive_subkey(const key_t& parent, uint8_t label, uint8_t algorithm, size_t size)
{
    if (size == 0)
    {
        return {};
    }

    const std::array<uint8_t, 2> info{label, algorithm};
    return truncate_key(noise_hkdf2(parent, bfc::const_buffer_view(
        reinterpret_cast<const std::byte*>(info.data()), info.size())).first,
        size);
}

} // namespace

network_derived_keys_s derive_network_keys(const key_t& base, uint8_t integrity_algorithm, uint8_t confidentiality_algorithm)
{
    return network_derived_keys_s{
        derive_subkey(base, k_label_integrity, integrity_algorithm, integrity_key_size(integrity_algorithm)),
        derive_subkey(base, k_label_confidentiality, confidentiality_algorithm, confidentiality_key_size(confidentiality_algorithm)),
    };
}

bool verify_integrity_mac(uint8_t integrity_algorithm, const key_t& key, bfc::const_buffer_view data, bfc::const_buffer_view mac)
{
    if (integrity_algorithm == E_EA_NONE)
    {
        return mac.empty();
    }

    if (key.empty() || mac.empty())
    {
        return false;
    }

    const size_t expected_size = integrity_mac_size(integrity_algorithm);
    if (mac.size() != expected_size)
    {
        return false;
    }

    key_t computed;
    switch (integrity_algorithm)
    {
        case E_EA_HMAC_SHA2_256:
            computed = hmac_sha256(key, data);
            break;
        case E_EA_HMAC_SHA2_512:
        case E_EA_HMAC_BLAKE3:
        default:
            return false;
    }

    if (computed.size() < expected_size)
    {
        return false;
    }

    computed.resize(expected_size);
    if (mac.size() != computed.size())
    {
        return false;
    }
    return std::memcmp(computed.data(), mac.data(), computed.size()) == 0;
}

bool verify_frame_mac(const frame_const_t& frame, bfc::const_buffer_view pdu, uint8_t integrity_algorithm, const key_t& integrity_key)
{
    const size_t mac_size = frame.get_mac_size();
    const size_t mac_offset = frame_const_t::k_fixed_prefix_size;

    if (mac_size == 0)
    {
        return integrity_algorithm == E_EA_NONE;
    }

    if (pdu.size() < mac_offset + mac_size)
    {
        return false;
    }

    std::vector<std::byte> authenticated;
    authenticated.reserve(pdu.size() - mac_size);
    authenticated.insert(authenticated.end(), pdu.data(), pdu.data() + mac_offset);
    authenticated.insert(authenticated.end(), pdu.data() + mac_offset + mac_size, pdu.data() + pdu.size());

    return verify_integrity_mac(
        integrity_algorithm,
        integrity_key,
        bfc::const_buffer_view(authenticated.data(), authenticated.size()),
        bfc::const_buffer_view(pdu.data() + mac_offset, mac_size));
}

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
