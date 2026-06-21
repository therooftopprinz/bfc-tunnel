#ifndef BFC_TUNNEL_TYPES_HPP
#define BFC_TUNNEL_TYPES_HPP

#include "bfc/function.hpp"
#include <cstdint>

#include <bfc/buffer.hpp>
#include <bfc/cv_reactor.hpp>
#include <bfc/default_reactor.hpp>
#include <bfc/socket.hpp>

namespace bfc_tunnel
{

constexpr uint64_t k_completion_code_success = 0;
constexpr uint64_t k_completion_code_failure = 1;

using cv_reactor_cb_t = std::function<void()>;
using io_reactor_cb_t = std::function<void()>;
using completion_cb_t = std::function<void(int code)>;

using reactor_cb_t     = cv_reactor_cb_t;
using cv_reactor_t     = bfc::cv_reactor<cv_reactor_cb_t>;
using io_reactor_t     = bfc::default_reactor<io_reactor_cb_t>;
using io_reactor_ptr_t = std::shared_ptr<io_reactor_t>;
using cv_reactor_ptr_t = std::shared_ptr<cv_reactor_t>;

using node_id_t = uint32_t;

using key_t = std::vector<uint8_t>;

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TYPES_HPP
