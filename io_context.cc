#include <signal.h>
#include <string.h>
#include "io_context.h"
#include "log.h"
#include "socket.h"
#include "timer.h"

namespace
{
class IgnoreSigPipe
{
public:
    IgnoreSigPipe()
    {
        ::signal(SIGPIPE, SIG_IGN);
        log_ok::file = stderr;
        LOG << "ignored SIGPIPE";
    }
};

IgnoreSigPipe initObj;

} // namespace

///   io_context
io_context::io_context()
{
    _epoll_fd = create_epoll_fd();
    _events.resize(64);
}

io_context::~io_context() {}

void io_context::update(int fd, int ev, call_back_type call)
{
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = fd;
    event.events = ev;

    int op = EPOLL_CTL_ADD;
    auto it = _calls.find(fd);
    if (it != _calls.end())
    {
        op = EPOLL_CTL_MOD;
    }
    _calls[fd] = call;

    if (ev == none_event)
    {
        op = EPOLL_CTL_DEL;
        _calls.erase(fd);
        LOG << "erase fd : " << fd;
    }
    ::epoll_ctl(_epoll_fd, op, fd, &event);
}

// void io_context::add_func(const call_back_type& call) {
// _pending_funcs.push_back(call);
//}

// loop
void io_context::run()
{
    while (!_calls.empty())
    {
        size_t active_num = ::epoll_wait(_epoll_fd, _events.data(), static_cast<int>(_events.size()), _epoll_time_out);
        if (active_num > 0)
        {
            call_list active_calls;

            get_active_event(active_num, &active_calls);

            // call
            event_call_back(active_calls);

            if (active_num == _events.size())
            {
                _events.resize(_events.size() * 2);
            }
        }
        call_pending_funcs(); // other
    }
}

void io_context::get_active_event(size_t active_number,
                                  call_list* active_calls)
{
    for (size_t i = 0; i < active_number; i++)
    {
        int fd = _events.at(i).data.fd;
        if (_calls.count(fd))
        {
            active_calls->insert({ fd, _calls[fd] });
        }
        else
        {
            LOG << "fd calls is null";
        }
    }
}

void io_context::event_call_back(const call_list& active_calls)
{
    for (const auto& it : active_calls)
    {
        it.second(); // call user func
    }
}

void io_context::call_pending_funcs()
{
    for (const auto& func : _pending_funcs)
    {
        func();
    }
    _pending_funcs.clear();
}
