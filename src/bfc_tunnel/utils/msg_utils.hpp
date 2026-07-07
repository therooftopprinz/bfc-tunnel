#ifndef BFC_TUNNEL_UTILS_MSG_UTILS_HPP
#define BFC_TUNNEL_UTILS_MSG_UTILS_HPP

#include <bfc_tunnel/protocol/btprotocol.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc_tunnel/security/key_utils.hpp>
#include <bfc/sized_buffer.hpp>

#include <array>
#include <cstddef>

namespace bfc_tunnel
{

frame_t prepare_frame(bfc::sized_buffer& buffer);
frame_const_t to_frame(const std::byte* data, size_t size);

template<typename T>
bool encode_payload(frame_t& frame, const T& msg)
{
    cum::per_codec_ctx ctx{frame.get_payload(), frame.get_payload_size()};
    try
    {
        cum::encode_per(msg, ctx);
    }
    catch (const std::exception& e)
    {
        return false;
    }

    frame.resize(frame.get_payload_size() - ctx.size());

    frame.set_payload_type(msg_id<T>::value);
    return true;
}

template<typename T>
key_t sign_btp_message(const T& msg, dh_key_type_e key_type, const key_t& private_key)
{
    T unsigned_msg = msg;
    unsigned_msg.signature = {};

    std::array<std::byte, 256> buf{};
    cum::per_codec_ctx ctx(buf.data(), buf.size());
    cum::encode_per(unsigned_msg, ctx);

    const auto view = bfc::const_buffer_view(buf.data(), buf.size() - ctx.size());
    return sign(key_type, private_key, view);
}

template<typename T>
bool verify_btp_message(const T& msg, dh_key_type_e key_type, const key_t& public_key)
{
    const key_t signature = msg.signature;

    T unsigned_msg = msg;
    unsigned_msg.signature = {};

    std::array<std::byte, 256> buf{};
    cum::per_codec_ctx ctx(buf.data(), buf.size());
    cum::encode_per(unsigned_msg, ctx);

    const auto view = bfc::const_buffer_view(buf.data(), buf.size() - ctx.size());
    return verify(key_type, public_key, view, signature);
}

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_UTILS_MSG_UTILS_HPP