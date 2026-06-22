#ifndef BFC_TUNNEL_SECURITY_DH_BACKEND_HPP
#define BFC_TUNNEL_SECURITY_DH_BACKEND_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>

namespace bfc_tunnel
{

class dh_backend
{
public:
    virtual ~dh_backend() = default;

    virtual size_t key_bytes() const = 0;
    virtual key_t  shared(const key_t& private_key, const key_t& peer_public_key) const = 0;
    virtual void   generate_ephemeral(key_t& private_key, key_t& public_key) const = 0;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_SECURITY_DH_BACKEND_HPP
