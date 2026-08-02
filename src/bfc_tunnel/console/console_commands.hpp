#ifndef BFC_TUNNEL_CONSOLE_COMMANDS_HPP
#define BFC_TUNNEL_CONSOLE_COMMANDS_HPP

#include <bfc_tunnel/console/parser.hpp>
#include <bfc_tunnel/node_runtime.hpp>

#include <string>

namespace bfc_tunnel
{
namespace console
{

// Returns unframed table body (caller applies frame_reply / \n\n).
std::string dispatch_command(node_runtime& runtime, const command_t& cmd);

} // namespace console
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_CONSOLE_COMMANDS_HPP
