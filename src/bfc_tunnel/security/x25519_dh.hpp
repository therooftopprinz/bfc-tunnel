#ifndef BFC_TUNNEL_SECURITY_X25519_DH_HPP
#define BFC_TUNNEL_SECURITY_X25519_DH_HPP

#include <bfc_tunnel/security/dh_backend.hpp>

namespace bfc_tunnel
{

class x25519_dh final : public dh_backend
{
public:
    static constexpr size_t k_key_bytes = 32;

    size_t key_bytes() const override { return k_key_bytes; }
    key_t  shared(const key_t& private_key, const key_t& peer_public_key) const override;
    void   generate_ephemeral(key_t& private_key, key_t& public_key) const override;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_SECURITY_X25519_DH_HPP
