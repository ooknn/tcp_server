/*

   copyright 2019 ggyyll

*/

#ifndef TIMER_H_
#define TIMER_H_

#include <functional>
#include <map>
#include <vector>
#include "noncopyable.h"

class io_context;

using fd_type = int;
using call_back_type = std::function<void()>;
class timer : noncopyable
{
public:
    explicit timer(io_context* context);
    ~timer();

    void set(uint64_t time_out, call_back_type call = call_back_type(), bool repeat = false);
    void cancle();

private:
    void enable_read();
    void set_timer_fd(int64_t when);
    void call_back();

private:
    int _fd;
    int64_t _t;
    bool _repeat;
    bool _enable;
    io_context* _context;
    call_back_type _call;
};
#endif // TIMER_H_
