#ifndef BFC_TUNNEL_SECURITY_DH_HPP
#define BFC_TUNNEL_SECURITY_DH_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/security/dh_backend.hpp>

#include <memory>

namespace bfc_tunnel
{

struct handshake_secrets_s
{
    key_t eb;
    key_t ab;
    key_t ef;
    key_t af;
};

class dhke_kk
{
public:
    explicit dhke_kk(std::unique_ptr<dh_backend> dh);

    void reset();

    void setup_handshake(const key_t& own_static_private, const key_t& peer_static_public, bool is_initiator);
    void set_own_private(const key_t& sk);
    void set_peer_public(const key_t& pk);
    void set_static_public_keys(const key_t& initiator_static_public, const key_t& responder_static_public);
    void set_initiator(bool is_initiator);
    void get_own_ephemeral_public(key_t& out);
    void set_peer_ephemeral_public(const key_t& pk);

    const key_t& own_ephemeral_public() const;

    key_t derive_integrity_key(uint8_t algorithm) const;
    key_t derive_confidentiality_key(uint8_t algorithm) const;
    key_t derive_receive_integrity_key(uint8_t algorithm) const;
    key_t derive_receive_confidentiality_key(uint8_t algorithm) const;

    handshake_secrets_s handshake_secrets() const;

    key_t dh_eb() const;
    key_t dh_ab() const;
    key_t dh_ef() const;
    key_t dh_af() const;

private:
    key_t send_traffic_key() const;
    key_t receive_traffic_key() const;
    std::pair<key_t, key_t> split_traffic_keys() const;
    key_t handshake_secret_eb() const;
    key_t handshake_secret_ab() const;
    key_t handshake_secret_ef() const;
    key_t handshake_secret_af() const;

    std::unique_ptr<dh_backend> dh_;
    key_t                       own_static_;
    key_t                       peer_static_;
    key_t                       initiator_static_public_;
    key_t                       responder_static_public_;
    key_t                       own_ephemeral_;
    key_t                       own_ephemeral_public_;
    key_t                       peer_ephemeral_;
    bool                        is_initiator_ = false;
};

std::unique_ptr<dh_backend> make_dh_backend(dh_key_type_e type);
dhke_kk                     make_dhke_kk(dh_key_type_e type);

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_SECURITY_DH_HPP
