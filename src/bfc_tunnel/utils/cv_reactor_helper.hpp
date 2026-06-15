#ifndef BFC_TUNNEL_CV_REACTOR_HELPER_HPP
#define BFC_TUNNEL_CV_REACTOR_HELPER_HPP

#include <future>

#include <bfc_tunnel/bfc_tunnel_types.hpp>

namespace bfc_tunnel
{
namespace utils
{

void dispatch_sync(cv_reactor_t& reactor, cv_reactor_cb_t cb);

inline void dispatch_sync(cv_reactor_t& reactor, cv_reactor_cb_t cb)
{
    if (!cb)
    {
        return;
    }

    if (reactor.is_reactor_thread())
    {
        cb();
        return;
    }

    std::promise<void> done;
    reactor.wake_up([&cb, &done]() {
        cb();
        done.set_value();
    });
    done.get_future().wait();
}

} // namespace utils
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_CV_REACTOR_HELPER_HPP
