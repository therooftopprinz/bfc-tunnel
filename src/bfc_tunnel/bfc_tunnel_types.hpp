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

class node;
struct peer_s;

constexpr uint64_t k_completion_code_success = 0;
constexpr uint64_t k_completion_code_failure = 1;

using cv_reactor_cb_t          = std::function<void()>;
using io_reactor_cb_t          = std::function<void()>;
using transaction_cb_t         = std::function<void(node&, const std::shared_ptr<peer_s>&, uint8_t id)>;
using completion_cb_t          = std::function<void(node&, const std::shared_ptr<peer_s>&, uint8_t id, int code)>;
using expiration_cb_t          = std::function<void(node&, const std::shared_ptr<peer_s>&, uint8_t id)>;

using procedure_completion_cb_t = std::function<void(node&, const std::shared_ptr<peer_s>&, int code)>;

using reactor_cb_t     = cv_reactor_cb_t;
using cv_reactor_t     = bfc::cv_reactor<cv_reactor_cb_t>;
using io_reactor_t     = bfc::default_reactor<io_reactor_cb_t>;
using io_reactor_ptr_t = std::shared_ptr<io_reactor_t>;
using cv_reactor_ptr_t = std::shared_ptr<cv_reactor_t>;

using timer_id_t = bfc::timer<>::timer_id_t;

using node_id_t = uint32_t;

using key_t = std::vector<uint8_t>;
using key_vec_t = std::vector<key_t>;

using u8_vec_t  = std::vector<uint8_t>;
using u32_vec_t = std::vector<uint32_t>;

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_TYPES_HPP
