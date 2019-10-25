#ifndef SOCKET_OPTS_H_
#define SOCKET_OPTS_H_
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/uio.h>
#include <unistd.h>

const int none_event = 0;
const int write_event = EPOLLOUT;
const int read_event = EPOLLIN | EPOLLPRI;

int create_timer_fd();
int create_epoll_fd();
int create_nonblocking_fd();

#endif // SOCKET_OPTS_H_
