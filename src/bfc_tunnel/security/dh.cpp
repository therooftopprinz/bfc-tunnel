#include <bfc_tunnel/security/dh.hpp>

#include <bfc_tunnel/security/x25519_dh.hpp>

#include <stdexcept>

namespace bfc_tunnel
{

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
    own_ephemeral_.clear();
    own_ephemeral_public_.clear();
    peer_ephemeral_.clear();
}

void dhke_kk::set_own_private(const key_t& sk) { own_static_ = sk; }
void dhke_kk::set_peer_public(const key_t& pk) { peer_static_ = pk; }

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
    }

    throw std::invalid_argument("unsupported dh key type");
}

dhke_kk make_dhke_kk(dh_key_type_e type)
{
    return dhke_kk(make_dh_backend(type));
}

} // namespace bfc_tunnel
