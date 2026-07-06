#include <bfc_tunnel/security/dh.hpp>

#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/security/x25519_dh.hpp>

#include <botan/hash.h>
#include <botan/mac.h>

#include <array>
#include <stdexcept>
#include <string_view>

namespace bfc_tunnel
{

namespace
{

constexpr std::string_view k_protocol_name = "BTF";
constexpr uint8_t          k_label_integrity       = 0x01;
constexpr uint8_t          k_label_confidentiality = 0x02;

key_t hash_bytes(bfc::const_buffer_view data)
{
    auto hash = Botan::HashFunction::create_or_throw("SHA-256");
    hash->update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    const auto digest = hash->final();
    return key_t(digest.begin(), digest.end());
}

key_t hash_concat(bfc::const_buffer_view left, bfc::const_buffer_view right)
{
    auto hash = Botan::HashFunction::create_or_throw("SHA-256");
    hash->update(reinterpret_cast<const uint8_t*>(left.data()), left.size());
    hash->update(reinterpret_cast<const uint8_t*>(right.data()), right.size());
    const auto digest = hash->final();
    return key_t(digest.begin(), digest.end());
}

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

void noise_mix_key(key_t& chaining_key, bfc::const_buffer_view input_key_material)
{
    chaining_key = noise_hkdf2(chaining_key, input_key_material).first;
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

dhke_kk::dhke_kk(std::unique_ptr<dh_backend> dh)
    : dh_(std::move(dh))
{
    if (!dh_)
    {
        throw std::invalid_argument("dh backend is required");
    }
}

void dhke_kk::reset()
{
    own_static_.clear();
    peer_static_.clear();
    initiator_static_public_.clear();
    responder_static_public_.clear();
    own_ephemeral_.clear();
    own_ephemeral_public_.clear();
    peer_ephemeral_.clear();
    is_initiator_ = false;
}

void dhke_kk::setup_handshake(const key_t& own_static_private, const key_t& peer_static_public, bool is_initiator)
{
    set_own_private(own_static_private);
    set_peer_public(peer_static_public);
    set_initiator(is_initiator);

    const key_t own_static_public = x25519_dh::public_from_private(own_static_private);
    if (is_initiator)
    {
        set_static_public_keys(own_static_public, peer_static_public);
    }
    else
    {
        set_static_public_keys(peer_static_public, own_static_public);
    }
}

void dhke_kk::set_own_private(const key_t& sk) { own_static_ = sk; }
void dhke_kk::set_peer_public(const key_t& pk) { peer_static_ = pk; }

void dhke_kk::set_static_public_keys(const key_t& initiator_static_public, const key_t& responder_static_public)
{
    initiator_static_public_ = initiator_static_public;
    responder_static_public_ = responder_static_public;
}

void dhke_kk::set_initiator(bool is_initiator) { is_initiator_ = is_initiator; }

void dhke_kk::get_own_ephemeral_public(key_t& out)
{
    dh_->generate_ephemeral(own_ephemeral_, out);
    own_ephemeral_public_ = out;
}

void dhke_kk::set_peer_ephemeral_public(const key_t& pk) { peer_ephemeral_ = pk; }

const key_t& dhke_kk::own_ephemeral_public() const { return own_ephemeral_public_; }

key_t dhke_kk::dh_eb() const { return dh_->shared(own_ephemeral_, peer_static_); }
key_t dhke_kk::dh_ab() const { return dh_->shared(own_static_, peer_static_); }
key_t dhke_kk::dh_ef() const { return dh_->shared(own_ephemeral_, peer_ephemeral_); }
key_t dhke_kk::dh_af() const { return dh_->shared(own_static_, peer_ephemeral_); }

key_t dhke_kk::handshake_secret_eb() const
{
    if (is_initiator_)
    {
        return dh_->shared(own_ephemeral_, peer_static_);
    }

    return dh_->shared(own_static_, peer_ephemeral_);
}

key_t dhke_kk::handshake_secret_ab() const { return dh_->shared(own_static_, peer_static_); }

key_t dhke_kk::handshake_secret_ef() const { return dh_->shared(own_ephemeral_, peer_ephemeral_); }

key_t dhke_kk::handshake_secret_af() const
{
    if (is_initiator_)
    {
        return dh_->shared(own_static_, peer_ephemeral_);
    }

    return dh_->shared(own_ephemeral_, peer_static_);
}

handshake_secrets_s dhke_kk::handshake_secrets() const
{
    return handshake_secrets_s{
        handshake_secret_eb(),
        handshake_secret_ab(),
        handshake_secret_ef(),
        handshake_secret_af(),
    };
}

std::pair<key_t, key_t> dhke_kk::split_traffic_keys() const
{
    key_t chaining_key = hash_bytes(bfc::const_buffer_view(
        reinterpret_cast<const std::byte*>(k_protocol_name.data()), k_protocol_name.size()));
    chaining_key = hash_concat(bfc::const_buffer_view(
                                   reinterpret_cast<const std::byte*>(chaining_key.data()), chaining_key.size()),
                               bfc::const_buffer_view(reinterpret_cast<const std::byte*>(initiator_static_public_.data()),
                                                      initiator_static_public_.size()));
    chaining_key = hash_concat(bfc::const_buffer_view(
                                   reinterpret_cast<const std::byte*>(chaining_key.data()), chaining_key.size()),
                               bfc::const_buffer_view(reinterpret_cast<const std::byte*>(responder_static_public_.data()),
                                                      responder_static_public_.size()));

    const key_t dh_eb = handshake_secret_eb();
    const key_t dh_ab = handshake_secret_ab();
    const key_t dh_ef = handshake_secret_ef();
    const key_t dh_af = handshake_secret_af();

    noise_mix_key(chaining_key, bfc::const_buffer_view(
        reinterpret_cast<const std::byte*>(dh_eb.data()), dh_eb.size()));
    noise_mix_key(chaining_key, bfc::const_buffer_view(
        reinterpret_cast<const std::byte*>(dh_ab.data()), dh_ab.size()));
    noise_mix_key(chaining_key, bfc::const_buffer_view(
        reinterpret_cast<const std::byte*>(dh_ef.data()), dh_ef.size()));
    noise_mix_key(chaining_key, bfc::const_buffer_view(
        reinterpret_cast<const std::byte*>(dh_af.data()), dh_af.size()));

    return noise_hkdf2(chaining_key, bfc::const_buffer_view{});
}

key_t dhke_kk::send_traffic_key() const
{
    const auto [send_key, receive_key] = split_traffic_keys();
    (void)receive_key;
    return is_initiator_ ? send_key : receive_key;
}

key_t dhke_kk::receive_traffic_key() const
{
    const auto [send_key, receive_key] = split_traffic_keys();
    (void)send_key;
    return is_initiator_ ? receive_key : send_key;
}

key_t dhke_kk::derive_integrity_key(uint8_t algorithm) const
{
    return derive_subkey(send_traffic_key(), k_label_integrity, algorithm, integrity_key_size(algorithm));
}

key_t dhke_kk::derive_confidentiality_key(uint8_t algorithm) const
{
    return derive_subkey(send_traffic_key(), k_label_confidentiality, algorithm, confidentiality_key_size(algorithm));
}

key_t dhke_kk::derive_receive_integrity_key(uint8_t algorithm) const
{
    return derive_subkey(receive_traffic_key(), k_label_integrity, algorithm, integrity_key_size(algorithm));
}

key_t dhke_kk::derive_receive_confidentiality_key(uint8_t algorithm) const
{
    return derive_subkey(receive_traffic_key(), k_label_confidentiality, algorithm, confidentiality_key_size(algorithm));
}

std::unique_ptr<dh_backend> make_dh_backend(dh_key_type_e type)
{
    switch (type)
    {
        case E_DHKT_X25519:
            return std::make_unique<x25519_dh>();
        case E_DHKT_NONE:
        case E_DHKT_SECP256R1:
        case E_DHKT_CURVE448:
            break;
        default:
            break;
    }

    return nullptr;
}

dhke_kk make_dhke_kk(dh_key_type_e type)
{
    return dhke_kk(make_dh_backend(type));
}

} // namespace bfc_tunnel
