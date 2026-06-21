#ifndef BFC_TUNNEL_REACTOR_HELPER_HPP
#define BFC_TUNNEL_REACTOR_HELPER_HPP

#include <future>
#include <type_traits>

namespace bfc_tunnel
{
namespace utils
{
namespace detail
{

template <typename T, typename = void>
struct has_bool_test : std::false_type
{};

template <typename T>
struct has_bool_test<T, std::void_t<decltype(static_cast<bool>(std::declval<const T&>()))>> : std::true_type
{};

} // namespace detail

template <typename reactor_t, typename cb_t>
inline void dispatch_sync(reactor_t& reactor, cb_t cb)
{
    if constexpr (detail::has_bool_test<cb_t>::value)
    {
        if (!cb)
        {
            return;
        }
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

#endif // BFC_TUNNEL_REACTOR_HELPER_HPP
