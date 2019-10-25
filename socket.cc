#include "socket.h"

int create_nonblocking_fd()
{
    int sock_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    return sock_fd;
}
int create_timer_fd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    return timerfd;
}

int create_epoll_fd()
{
    int epfd = ::epoll_create1(EPOLL_CLOEXEC);
    return epfd;
}
