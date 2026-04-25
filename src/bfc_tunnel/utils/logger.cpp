#include <bfc_tunnel/logger.hpp>

#include <cstring>

namespace bfc_tunnel
{

logger::logger(const std::string& filename)
{
    logfile.open(filename, std::ios::out | std::ios::app);
}

logger::~logger()
{
    logfile.close();
}

void logger::log(const char* message)
{
    logfile.write(message, strlen(message));
    logfile.put('\n');
}

void logger::flush()
{
    logfile.flush();
}

void logger::set_logbit(uint64_t logbit)
{
    this->logbit.fetch_or(logbit, std::memory_order_relaxed);
}

void logger::clear_logbit(uint64_t logbit)
{
    this->logbit.fetch_and(~logbit, std::memory_order_relaxed);
}

uint64_t logger::get_logbit() const
{
    return logbit.load(std::memory_order_relaxed);
}

void logger::set_log_stdout(bool log_stdout)
{
    this->log_stdout.store(log_stdout, std::memory_order_relaxed);
}


} // namespace bfc_tunnel