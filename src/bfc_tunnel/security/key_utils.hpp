#ifndef BFC_TUNNEL_SECURITY_KEY_UTILS_HPP
#define BFC_TUNNEL_SECURITY_KEY_UTILS_HPP

#include <bfc/buffer.hpp>
#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/frame.hpp>

#include <utility>

namespace bfc_tunnel
{

struct network_derived_keys_s
{
    key_t integrity_key;
    key_t confidentiality_key;
};

key_t sign(dh_key_type_e key_type, const key_t& private_key, bfc::const_buffer_view message);
bool verify(dh_key_type_e key_type, const key_t& public_key, bfc::const_buffer_view message, const key_t& signature);
network_derived_keys_s derive_network_keys(const key_t& base, uint8_t integrity_algorithm, uint8_t confidentiality_algorithm);
bool verify_integrity_mac(uint8_t integrity_algorithm, const key_t& key, bfc::const_buffer_view data, bfc::const_buffer_view mac);
bool verify_frame_mac(const frame_const_t& frame, bfc::const_buffer_view pdu, uint8_t integrity_algorithm, const key_t& integrity_key);

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_SECURITY_KEY_UTILS_HPP