#include "bfc_tunnel/utils/msg_utils.hpp"
#include <chrono>

namespace bfc_tunnel
{

frame_t prepare_frame(bfc::sized_buffer& buffer)
{
    frame_t frame{buffer.data(), buffer.size()};
    frame.set_reserved(0);
    frame.set_version(1);
    frame.set_sec_ctx(k_sec_ctx_none);
    frame.set_mac_size_units(0);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    frame.set_ts(now);
    return frame;
}

frame_const_t to_frame(const std::byte* data, size_t size)
{
    return frame_const_t(data, size);
}

} // namespace bfc_tunnel