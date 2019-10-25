#include <mutex>
#include <unistd.h>
#include <string.h>
#include <iomanip>
#include "log.h"

namespace log
{
static std::mutex mtx;
FILE* file;

log::log(const char* file,const char*func, const int line)
{
    time_t tim = time(NULL);
    struct tm t;
    {
        std::lock_guard<std::mutex> lock(mtx);
        t = *localtime(&tim);
    }
    char buf[16];
    snprintf(buf, sizeof buf, "%02d:%02d:%02d ", t.tm_hour, t.tm_min, t.tm_sec);
    stream_ << buf;
    {
        const char* p = strrchr(file, '/');
        if (p)
        {
            file = p + 1;
        }
        stream_ << std::right << std::setw(15) << file << ':' << std::left << std::setw(3) ;
        stream_ << std::left << std::setw(15) << func << ':' << std::left << std::setw(3) << line;
    }
    stream_ << ' ';
}

log::~log()
{
    if (!file)
        return;
    std::lock_guard<std::mutex> lock(mtx);
    stream_ << '\n';
    fputs(stream_.str().c_str(), file);
    fflush(file);
}
} // namespace help
