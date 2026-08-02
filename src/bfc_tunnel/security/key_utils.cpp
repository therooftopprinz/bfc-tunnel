#include <bfc_tunnel/security/key_utils.hpp>

#include <bfc_tunnel/security/x25519_dh.hpp>

#include <botan/ed25519.h>
#include <botan/block_cipher.h>
#include <botan/mac.h>
#include <botan/pubkey.h>
#include <botan/stream_cipher.h>
#include <botan/system_rng.h>

#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
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

key_t compute_integrity_mac(uint8_t integrity_algorithm, const key_t& key, bfc::const_buffer_view data)
{
    if (integrity_algorithm == E_EA_NONE)
    {
        return {};
    }

    if (key.empty())
    {
        return {};
    }

    const size_t expected_size = integrity_mac_size(integrity_algorithm);
    if (expected_size == 0)
    {
        return {};
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
            return {};
    }

    if (computed.size() < expected_size)
    {
        return {};
    }

    computed.resize(expected_size);
    return computed;
}

bool verify_integrity_mac(uint8_t integrity_algorithm, const key_t& key, bfc::const_buffer_view data, bfc::const_buffer_view mac)
{
    if (integrity_algorithm == E_EA_NONE)
    {
        return mac.empty();
    }

    if (mac.empty())
    {
        return false;
    }

    const key_t computed = compute_integrity_mac(integrity_algorithm, key, data);
    if (computed.empty() || mac.size() != computed.size())
    {
        return false;
    }
    return std::memcmp(computed.data(), mac.data(), computed.size()) == 0;
}

namespace
{

std::vector<std::byte> frame_authenticated_bytes(const frame_const_t& frame, bfc::const_buffer_view pdu)
{
    const size_t mac_size = frame.get_mac_size();
    const size_t mac_offset = frame_const_t::k_fixed_prefix_size;
    if (pdu.size() < mac_offset + mac_size)
    {
        return {};
    }

    std::vector<std::byte> authenticated;
    authenticated.reserve(pdu.size() - mac_size);
    authenticated.insert(authenticated.end(), pdu.data(), pdu.data() + mac_offset);
    authenticated.insert(authenticated.end(), pdu.data() + mac_offset + mac_size, pdu.data() + pdu.size());
    return authenticated;
}

} // namespace

bool protect_frame_mac(frame_t& frame, uint8_t integrity_algorithm, const key_t& integrity_key)
{
    const size_t mac_size = frame.get_mac_size();
    const size_t expected_size = integrity_mac_size(integrity_algorithm);
    if (mac_size != expected_size)
    {
        return false;
    }

    if (mac_size == 0)
    {
        return integrity_algorithm == E_EA_NONE;
    }

    const frame_const_t cframe(frame.get_base(), frame.get_size());
    const bfc::const_buffer_view pdu(frame.get_base(), frame.get_size());
    const auto authenticated = frame_authenticated_bytes(cframe, pdu);
    if (authenticated.size() != pdu.size() - mac_size)
    {
        return false;
    }

    const key_t mac = compute_integrity_mac(
        integrity_algorithm,
        integrity_key,
        bfc::const_buffer_view(authenticated.data(), authenticated.size()));
    if (mac.size() != mac_size)
    {
        return false;
    }

    std::memcpy(frame.get_mac(), mac.data(), mac_size);
    return true;
}

bool verify_frame_mac(const frame_const_t& frame, bfc::const_buffer_view pdu, uint8_t integrity_algorithm, const key_t& integrity_key)
{
    const size_t mac_size = frame.get_mac_size();

    if (mac_size == 0)
    {
        return integrity_algorithm == E_EA_NONE;
    }

    const auto authenticated = frame_authenticated_bytes(frame, pdu);
    if (authenticated.empty())
    {
        return false;
    }

    return verify_integrity_mac(
        integrity_algorithm,
        integrity_key,
        bfc::const_buffer_view(authenticated.data(), authenticated.size()),
        bfc::const_buffer_view(pdu.data() + frame_const_t::k_fixed_prefix_size, mac_size));
}

namespace
{

std::array<uint8_t, 16> frame_cipher_iv(const frame_const_t& frame)
{
    std::array<uint8_t, 16> iv{};
    const auto put_u32 = [&](size_t offset, uint32_t value)
    {
        iv[offset]     = static_cast<uint8_t>((value >> 24) & 0xFF);
        iv[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
        iv[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
        iv[offset + 3] = static_cast<uint8_t>(value & 0xFF);
    };
    put_u32(0, frame.get_sn());
    put_u32(4, frame.get_ts());
    put_u32(8, frame.get_src());
    return iv;
}

void ctr_xor_inplace(Botan::BlockCipher& cipher, std::array<uint8_t, 16> counter, uint8_t* data, size_t len)
{
    std::array<uint8_t, 16> keystream{};
    size_t offset = 0;
    while (offset < len)
    {
        cipher.encrypt(counter.data(), keystream.data());
        const size_t n = std::min<size_t>(16, len - offset);
        for (size_t i = 0; i < n; ++i)
        {
            data[offset + i] ^= keystream[i];
        }
        offset += n;

        for (int i = 15; i >= 0; --i)
        {
            if (++counter[static_cast<size_t>(i)] != 0)
            {
                break;
            }
        }
    }
}

bool cipher_frame_payload(frame_t& frame, uint8_t confidentiality_algorithm, const key_t& confidentiality_key)
{
    if (confidentiality_algorithm == E_CA_NONE)
    {
        return true;
    }

    const size_t expected_key_size = confidentiality_key_size(confidentiality_algorithm);
    if (expected_key_size == 0 || confidentiality_key.size() != expected_key_size)
    {
        return false;
    }

    const size_t payload_size = frame.get_payload_size();
    if (payload_size == 0)
    {
        return true;
    }

    auto* payload = reinterpret_cast<uint8_t*>(frame.get_payload());
    const auto iv = frame_cipher_iv(frame_const_t(frame.get_base(), frame.get_size()));

    try
    {
        if (confidentiality_algorithm == E_CA_CHACHA20)
        {
            auto cipher = Botan::StreamCipher::create_or_throw("ChaCha20");
            cipher->set_key(reinterpret_cast<const uint8_t*>(confidentiality_key.data()), confidentiality_key.size());
            // ChaCha20 accepts 8/12/24-byte nonces; use first 12 bytes of frame IV.
            cipher->set_iv(iv.data(), 12);
            cipher->cipher1(payload, payload_size);
            return true;
        }

        const char* aes_name = nullptr;
        if (confidentiality_algorithm == E_CA_AES128)
        {
            aes_name = "AES-128";
        }
        else if (confidentiality_algorithm == E_CA_AES256)
        {
            aes_name = "AES-256";
        }
        else
        {
            return false;
        }

        // Minimized Botan build includes AES block cipher but not CTR mode; implement CTR here.
        auto cipher = Botan::BlockCipher::create_or_throw(aes_name);
        cipher->set_key(reinterpret_cast<const uint8_t*>(confidentiality_key.data()), confidentiality_key.size());
        ctr_xor_inplace(*cipher, iv, payload, payload_size);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

} // namespace

bool encrypt_frame_payload(frame_t& frame, uint8_t confidentiality_algorithm, const key_t& confidentiality_key)
{
    return cipher_frame_payload(frame, confidentiality_algorithm, confidentiality_key);
}

bool decrypt_frame_payload(frame_t& frame, uint8_t confidentiality_algorithm, const key_t& confidentiality_key)
{
    return cipher_frame_payload(frame, confidentiality_algorithm, confidentiality_key);
}

bool protect_frame(frame_t& frame,
                   uint8_t integrity_algorithm,
                   const key_t& integrity_key,
                   uint8_t confidentiality_algorithm,
                   const key_t& confidentiality_key)
{
    if (!encrypt_frame_payload(frame, confidentiality_algorithm, confidentiality_key))
    {
        return false;
    }
    return protect_frame_mac(frame, integrity_algorithm, integrity_key);
}

bool accept_frame(frame_t& frame,
                  uint8_t integrity_algorithm,
                  const key_t& integrity_key,
                  uint8_t confidentiality_algorithm,
                  const key_t& confidentiality_key)
{
    const frame_const_t cframe(frame.get_base(), frame.get_size());
    const bfc::const_buffer_view pdu(frame.get_base(), frame.get_size());
    if (!verify_frame_mac(cframe, pdu, integrity_algorithm, integrity_key))
    {
        return false;
    }
    return decrypt_frame_payload(frame, confidentiality_algorithm, confidentiality_key);
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
