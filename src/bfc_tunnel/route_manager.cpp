#include <bfc_tunnel/route_manager.hpp>

#include <unordered_map>

namespace bfc_tunnel
{
namespace
{
bool route_entries_equal(const route_entry_s& a, const route_entry_s& b)
{
    return a.origin == b.origin && a.next == b.next && a.target == b.target && a.metric == b.metric;
}

/** For one update batch, keep the best (lowest metric) offer per target; first entry wins ties. */
std::unordered_map<node_id_s, route_entry_s> best_per_target(const std::vector<route_entry_s>& routes)
{
    std::unordered_map<node_id_s, route_entry_s> out;
    out.reserve(routes.size());
    for (const route_entry_s& e : routes)
    {
        auto it = out.find(e.target);
        if (it == out.end())
        {
            out.emplace(e.target, e);
        }
        else if (e.metric < it->second.metric)
        {
            it->second = e;
        }
    }
    return out;
}
} // namespace

route_manager::route_manager() = default;

route_manager::~route_manager() = default;

bool route_manager::update_route(const std::vector<route_entry_s>& routes)
{
    std::unordered_map<node_id_s, route_entry_s> batch = best_per_target(routes);

    std::vector<route_entry_s> delta;
    delta.reserve(batch.size());

    bool changed = false;
    for (const auto& kv : batch)
    {
        const route_entry_s& entry = kv.second;
        auto it = routes_by_target.find(entry.target);
        if (it == routes_by_target.end() || !route_entries_equal(it->second, entry))
        {
            routes_by_target[entry.target] = entry;
            delta.push_back(entry);
            changed = true;
        }
    }

    last_update_delta = std::move(delta);
    return changed;
}

const std::vector<route_entry_s>& route_manager::get_last_update_delta() const
{
    return last_update_delta;
}

const std::unordered_map<node_id_s, route_entry_s>& route_manager::get_routes() const
{
    return routes_by_target;
}

} // namespace bfc_tunnel
