#ifndef BFC_TUNNEL_UTILS_NUMBER_UTILS_HPP
#define BFC_TUNNEL_UTILS_NUMBER_UTILS_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>

#include <botan/system_rng.h>

#include <cstdint>
#include <random>
#include <set>
#include <vector>

namespace bfc_tunnel
{

inline uint64_t random_u64()
{
    static std::random_device rd;
    static std::mt19937_64    gen(rd());
    return gen();
}

inline key_t random_bytes(size_t size)
{
    key_t out(size);
    Botan::system_rng().randomize(out.data(), out.size());
    return out;
}


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