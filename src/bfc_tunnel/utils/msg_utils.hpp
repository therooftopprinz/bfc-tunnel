#ifndef BFC_TUNNEL_UTILS_MSG_UTILS_HPP
#define BFC_TUNNEL_UTILS_MSG_UTILS_HPP

#include <bfc_tunnel/protocol/btprotocol.hpp>
#include <bfc_tunnel/protocol/frame.hpp>
#include <bfc/sized_buffer.hpp>

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

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_UTILS_MSG_UTILS_HPP