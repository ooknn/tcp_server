#include "error.h"
std::string error_message()
{
    char err_buf[512];
    return strerror_r(errno, err_buf, sizeof err_buf);
}
