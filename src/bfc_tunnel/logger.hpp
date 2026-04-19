#ifndef BFC_TUNNEL_LOGGER_HPP
#define BFC_TUNNEL_LOGGER_HPP

#include <cstdio>
#include <fstream>
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

    void log(const char* message) const;
    void flush() const;

    void set_logbit(uint64_t logbit);
    void clear_logbit(uint64_t logbit);
    uint64_t get_logbit() const;
    void set_log_stdout(bool log_stdout);

private:
    std::ofstream logfile;
    std::atomic_uint64_t logbit = E_LOG_BIT_CRITICAL | E_LOG_BIT_ERROR;
    std::atomic_bool log_stdout = true;
};

template <typename... Args>
void log(logger& logger, uint64_t logbit, const char* format, Args... args)
{
    if ((logger.get_logbit() & logbit) != logbit)
    {
        return;
    }

    thread_local static char rawline[1024*512];
    thread_local static char fmtline[1024*512];;

    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto now_s  = now_ms / 1000;
    char timestamp[64];
    struct tm tm;
    localtime_r(&now_s, &tm);
    auto res = strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);
    timestamp[res] = '\0';

    res = snprintf(rawline, sizeof(rawline), format, args...);
    rawline[res] = '\0';
    res = snprintf(fmtline, sizeof(fmtline), "%s.%d %x08 %p %s", timestamp, now_ms%1000, logbit, std::this_thread::get_id(), rawline);
    fmtline[res] = '\0';

    logger.log(fmtline);
    if (logger.get_logbit() & (E_LOG_BIT_SHOULD_NOT_HAPPEN|E_LOG_BIT_CRITICAL|E_LOG_BIT_ERROR))
    {
        logger.flush();
    }
}

#define IF_LB(logbit) if ((logger.get_logbit() & logbit) == logbit)

inline logger tlogger;

} // namespace bfc_tunnel