#include "socket.h"
#include <string.h>
#include <assert.h>
#include "acceptor.h"
#include "io_context.h"
#include "log.h"
#include "error.h"

// acceptor

acceptor::acceptor(io_context* context, const uint16_t port)
    : _context(context)
    , _port(port)
{
    _fd = create_nonblocking_fd();
    reuse_addr();
    bind();
}

acceptor::~acceptor()
{
    if (_listening)
    {
        cancle();
    }
}

void acceptor::cancle()
{
    _context->update(_fd, none_event, std::bind(&acceptor::accept_back, this));
    close(_fd);
}

void acceptor::update()
{
    _context->update(_fd, read_event, std::bind(&acceptor::accept_back, this));
}

void acceptor::bind()
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htobe32(INADDR_ANY);
    addr.sin_port = htobe16(_port);
    int ret = ::bind(_fd, static_cast<struct sockaddr*>(static_cast<void*>(&addr)), static_cast<socklen_t>(sizeof(struct sockaddr_in)));
    if (ret != 0)
    {
        LOG << "system error " << error_message();
        assert(false);
    }
}

void acceptor::listen()
{
    int ret = ::listen(_fd, SOMAXCONN);
    if (ret != 0)
    {
        LOG << "system error " << error_message();
        assert(false);
    }
    _listening = true;
}

void acceptor::reuse_port()
{
    int opt = 1;
    ::setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &opt, static_cast<socklen_t>(sizeof opt));
}

void acceptor::reuse_addr()
{
    int opt = 1;
    ::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, static_cast<socklen_t>(sizeof opt));
}

void acceptor::accept_back()
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof addr);
    int conn_fd = ::accept4(_fd, static_cast<struct sockaddr*>(static_cast<void*>(&addr)), &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (conn_fd < 0)
    {
        LOG << "accpet system error " << error_message();
    }
    else
    {
        _on_connect(conn_fd);
    }
}
