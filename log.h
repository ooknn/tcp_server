/*

   copyright 2019 ggyyll

*/


#ifndef LOG_H_
#define LOG_H_

#include <sstream>
#include <stdio.h>

namespace log_ok
{
extern FILE* file;
struct log
{
    std::stringstream stream_;
    log(const char* file,const char*func, const int line);
    ~log();
};

} // namespace help

#define LOG log_ok::log(__FILE__,__FUNCTION__, __LINE__).stream_

#endif // LOG_H_
