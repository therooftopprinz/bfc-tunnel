#ifndef BFC_TUNNEL_ROUTE_MANAGER_HPP
#define BFC_TUNNEL_ROUTE_MANAGER_HPP

#include <bfc_tunnel/bfc_tunnel_types.hpp>

#include <unordered_map>
#include <vector>

namespace bfc_tunnel
{

struct route_entry_s
{
    node_id_s origin;
    node_id_s next;
    node_id_s target;
    uint16_t  metric;
};

class route_manager
{
public:
    route_manager();
    ~route_manager();

    // Update the routes from the given vector of route entries. Return true if the routes are updated, false otherwise.
    bool update_route(const std::vector<route_entry_s>& routes);
    // Get the last update delta of the routes.
    const std::vector<route_entry_s>& get_last_update_delta() const;
    // Get the snapshot of the routes.
    const std::unordered_map<node_id_s, route_entry_s>& get_routes() const;

private:
    std::unordered_map<node_id_s, route_entry_s> routes_by_target;
    std::vector<route_entry_s>                   last_update_delta;
};

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_ROUTE_MANAGER_HPP