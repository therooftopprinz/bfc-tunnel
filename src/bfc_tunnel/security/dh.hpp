#ifndef BFC_TUNNEL_SECURITY_DH_HPP
#define BFC_TUNNEL_SECURITY_DH_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/security/dh_backend.hpp>

#include <memory>

namespace bfc_tunnel
{

class dhke_kk
{
public:
    explicit dhke_kk(std::unique_ptr<dh_backend> dh);

    void reset();

    void set_own_private(const key_t& sk);
    void set_peer_public(const key_t& pk);
    void get_own_ephemeral_public(key_t& out);
    void set_peer_ephemeral_public(const key_t& pk);

    const key_t& own_ephemeral_public() const;

    key_t dh_eb() const;
    key_t dh_ab() const;
    key_t dh_ef() const;
    key_t dh_af() const;

private:
    std::unique_ptr<dh_backend> dh_;
    key_t                       own_static_;
    key_t                       peer_static_;
    key_t                       own_ephemeral_;
    key_t                       own_ephemeral_public_;
    key_t                       peer_ephemeral_;
};

std::unique_ptr<dh_backend> make_dh_backend(dh_key_type_e type);
dhke_kk                     make_dhke_kk(dh_key_type_e type);

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_SECURITY_DH_HPP
