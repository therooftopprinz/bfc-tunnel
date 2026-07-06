#ifndef BFC_TUNNEL_SECURITY_KEY_UTILS_HPP
#define BFC_TUNNEL_SECURITY_KEY_UTILS_HPP

#include <bfc/buffer.hpp>
#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/frame.hpp>

namespace bfc_tunnel
{

key_t sign(dh_key_type_e key_type, const key_t& private_key, bfc::const_buffer_view message);
bool verify(dh_key_type_e key_type, const key_t& public_key, bfc::const_buffer_view message, const key_t& signature);
 
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_SECURITY_KEY_UTILS_HPP