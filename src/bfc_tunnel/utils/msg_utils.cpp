#include "bfc_tunnel/utils/msg_utils.hpp"
#include <chrono>

namespace bfc_tunnel
{

frame_t prepare_frame(bfc::sized_buffer& buffer, uint8_t ctx_id, size_t mac_size)
{
    frame_t frame{buffer.data(), buffer.size(), mac_size};
    frame.set_reserved(0);
    frame.set_version(1);
    frame.set_sec_ctx(ctx_id);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    frame.set_ts(now);
    return frame;
}

frame_const_t to_frame(const std::byte* data, size_t size)
{
    frame_const_t probe(data, size);
    auto mac_size = integrity_mac_size(probe.get_int_algo());
    return frame_const_t(data, size, mac_size);
}

} // namespace bfc_tunnel