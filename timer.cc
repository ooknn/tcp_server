#include <assert.h>
#include <chrono>
#include <errno.h>
#include <string.h>
#include "timer.h"
#include "io_context.h"
#include "log.h"
#include "socket.h"
#include "error.h"

static auto now_ok() -> decltype(std::chrono::system_clock::now().time_since_epoch())
{
    return std::chrono::system_clock::now().time_since_epoch();
}

static uint64_t now_mic()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(now_ok()).count();
}

static const uint64_t second_to_microsecond = 1000 * 1000;

timer::timer(io_context* context)
    : _context(context)
{
    _fd = create_timer_fd();
}

timer::~timer()
{
    if (_enable)
    {
        cancle();
    }
    close(_fd);
};

void timer::enable_read()
{
    _context->update(_fd, read_event, std::bind(&timer::call_back, this));
    _enable = true;
}

void timer::cancle()
{
    _context->update(_fd, none_event, call_back_type());
    _enable = false;
}

void timer::set_timer_fd(int64_t when)
{
    int64_t microseconds = when - now_mic();
    assert(microseconds > 0);
    struct itimerspec new_value;
    memset(&new_value, 0, sizeof(new_value));
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / second_to_microsecond);
    ts.tv_nsec = static_cast<long>((microseconds % second_to_microsecond) * 1000);
    new_value.it_value = ts;
    int ret = ::timerfd_settime(_fd, 0, &new_value, NULL);
    if (ret)
    {
        LOG << error_message();
    }
}

void timer::set(uint64_t seconds, call_back_type call, bool repeat)
{
    _t = seconds;
    _repeat = repeat;
    _call = call;
    int64_t microseconds = now_mic() + seconds * second_to_microsecond;

    set_timer_fd(microseconds);
    enable_read();
}
void timer::call_back()
{
    cancle();
    _call();
    if (!_repeat)
    {
        return;
    }
    set(_t, _call, _repeat);
}
