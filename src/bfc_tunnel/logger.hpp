#ifndef BFC_TUNNEL_LOGGER_HPP
#define BFC_TUNNEL_LOGGER_HPP

#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <functional>
#include <string>
#include <ctime>
#include <chrono>
#include <thread>
#include <atomic>


namespace bfc_tunnel
{

constexpr uint64_t E_LOG_BIT_DEBUG             = 1 << 0;
constexpr uint64_t E_LOG_BIT_INFO              = 1 << 1;
constexpr uint64_t E_LOG_BIT_WARNING           = 1 << 2;
constexpr uint64_t E_LOG_BIT_ERROR             = 1 << 3;
constexpr uint64_t E_LOG_BIT_CRITICAL          = 1 << 4;
constexpr uint64_t E_LOG_BIT_SHOULD_NOT_HAPPEN = 1 << 5;

class logger
{
public:

    logger(const std::string& filename);
    ~logger();

    void log(const char* message);
    void flush();

    void set_logbit(uint64_t logbit);
    void clear_logbit(uint64_t logbit);
    uint64_t get_logbit() const;
    void set_log_stdout(bool log_stdout);

private:
    std::ofstream logfile;
    std::atomic_uint64_t logbit = E_LOG_BIT_CRITICAL | E_LOG_BIT_ERROR;
    std::atomic_bool log_stdout = true;
};

namespace detail
{

inline bool log_enabled(const logger& lg, uint64_t logbit)
{
    return (lg.get_logbit() & logbit) == logbit;
}

inline void log_line_and_maybe_flush(logger& lg, const char* message)
{
    lg.log(message);
    if (lg.get_logbit() & (E_LOG_BIT_SHOULD_NOT_HAPPEN | E_LOG_BIT_CRITICAL | E_LOG_BIT_ERROR))
    {
        lg.flush();
    }
}

#if defined(__GNUC__) || defined(__clang__)
#define BFC_TUNNEL_LOG_PRINTF_ATTR __attribute__((format(printf, 3, 4)))
#else
#define BFC_TUNNEL_LOG_PRINTF_ATTR
#endif

BFC_TUNNEL_LOG_PRINTF_ATTR
inline void log_with_prefix(logger& lg, uint64_t logbit, const char* format, ...)
{
    thread_local static char rawline[1024 * 512];
    thread_local static char fmtline[1024 * 512];

    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto now_s = now_ms / 1000;
    char timestamp[64];
    struct tm tm;
    localtime_r(&now_s, &tm);
    auto ts_len = strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);
    timestamp[ts_len] = '\0';

    va_list ap;
    va_start(ap, format);
    const int raw_len = std::vsnprintf(rawline, sizeof(rawline), format, ap);
    va_end(ap);
    if (raw_len < 0)
    {
        rawline[0] = '\0';
    }

    const auto thr_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    std::snprintf(fmtline, sizeof(fmtline), "%s.%03d %08" PRIx64 " %zu %s", timestamp,
                  static_cast<int>(now_ms % 1000), logbit, thr_hash, rawline);

    log_line_and_maybe_flush(lg, fmtline);
}

#undef BFC_TUNNEL_LOG_PRINTF_ATTR

} // namespace detail

inline void log(logger& lg, uint64_t logbit, const char* format)
{
    if (!detail::log_enabled(lg, logbit))
    {
        return;
    }
    detail::log_line_and_maybe_flush(lg, format);
}

template <typename... Args>
void log(logger& lg, uint64_t logbit, const char* format, Args... args)
{
    if (!detail::log_enabled(lg, logbit))
    {
        return;
    }
    detail::log_with_prefix(lg, logbit, format, args...);
}

#define IF_LB(logbit) if ((g_logger->get_logbit() & logbit) == logbit)

inline std::unique_ptr<logger> g_logger;

} // namespace bfc_tunnel

#endif // BFC_TUNNEL_LOGGER_HPP