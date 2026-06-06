#ifndef BFC_TUNNEL_TYPES_HPP
#define BFC_TUNNEL_TYPES_HPP

#include "bfc/function.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>

#include <bfc/buffer.hpp>
#include <bfc/cv_reactor.hpp>
#include <bfc/default_reactor.hpp>
#include <bfc/socket.hpp>

namespace bfc_tunnel
{

using cv_reactor_cb_t = typename bfc::function_type_helper<64, void()>::type;
using io_reactor_cb_t = std::function<void()>;

using reactor_cb_t = cv_reactor_cb_t;
using cv_reactor_t = bfc::cv_reactor<cv_reactor_cb_t>;
using io_reactor_t = bfc::default_reactor<io_reactor_cb_t>;
using io_reactor_ptr_t = std::shared_ptr<io_reactor_t>;
using cv_reactor_ptr_t = std::shared_ptr<cv_reactor_t>;

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TYPES_HPP
