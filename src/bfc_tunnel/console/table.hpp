#ifndef BFC_TUNNEL_CONSOLE_TABLE_HPP
#define BFC_TUNNEL_CONSOLE_TABLE_HPP

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace bfc_tunnel
{
namespace console
{

inline std::string frame_reply(const std::string& body)
{
    return body + "\n\n";
}

inline std::string render_table(const std::vector<std::string>& columns,
                                const std::vector<std::vector<std::string>>& rows)
{
    std::ostringstream out;
    out << '|';
    for (const auto& col : columns)
    {
        out << ' ' << col << " |";
    }
    out << '\n';
    for (const auto& row : rows)
    {
        out << '|';
        for (size_t i = 0; i < columns.size(); ++i)
        {
            out << ' ' << (i < row.size() ? row[i] : "") << " |";
        }
        out << '\n';
    }
    std::string s = out.str();
    if (!s.empty() && s.back() == '\n')
    {
        s.pop_back();
    }
    return s;
}

inline std::string status_ok()
{
    return render_table({"STATUS"}, {{"OK"}});
}

inline std::string status_err(const std::string& message)
{
    return render_table({"STATUS", "MESSAGE"}, {{"ERR", message}});
}

} // namespace console
} // namespace bfc_tunnel

#endif // BFC_TUNNEL_CONSOLE_TABLE_HPP
