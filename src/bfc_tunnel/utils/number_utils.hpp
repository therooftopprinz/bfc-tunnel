#ifndef BFC_TUNNEL_UTILS_NUMBER_UTILS_HPP
#define BFC_TUNNEL_UTILS_NUMBER_UTILS_HPP

#include <vector>
#include <set>

namespace bfc_tunnel
{

template<typename T>
std::set<std::pair<T,T>> get_pairs(std::vector<T> a, std::vector<T> b)
{
    std::set<std::pair<T,T>> pairs;
    for (const auto& a : a)
    {
        for (const auto& b : b)
        {
            pairs.insert(std::make_pair(a, b));
        }   
    }
    return pairs;
}

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_UTILS_NUMBER_UTILS_HPP