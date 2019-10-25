/*

   copyright 2019 ggyyll

*/

#ifndef IO_CONTEXT_H_
#define IO_CONTEXT_H_

#include <functional>
#include <map>
#include <vector>
#include "noncopyable.h"

using call_back_type = std::function<void()>;
using event_list = std::vector<struct epoll_event>;
using call_list = std::map<int, call_back_type>;

class io_context : noncopyable
{
public:
    io_context();
    ~io_context();
    void run();
    void add_func(const call_back_type& call);
    void update(int fd, int event, call_back_type call);

private:
    void call_pending_funcs();
    void event_call_back(const call_list& active_calls);
    void get_active_event(size_t active_number, call_list* active_calls);

private:
    int _epoll_fd;
    event_list _events;
    call_list _calls; // timerfd or socket fd
    std::vector<call_back_type> _pending_funcs; //  other  function
    int _epoll_time_out = -1;
};

#endif // IO_CONTEXT_H_
