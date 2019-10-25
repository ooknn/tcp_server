#include "connector.h"
#include "io_context.h"
#include "socket.h"
#include "netinet/in.h"
#include "log.h"
#include "error.h"
#include <string.h>

connector::connector(io_context* context, const std::string& ip, uint16_t port)
    : ip_(ip)
    , port_(port)
    , delay_(1)
    , context_(context)
    , t_(context)
{
}

connector::~connector()
{
}

void connector::start()
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (::inet_pton(AF_INET, ip_.data(), &addr.sin_addr) <= 0)
    {
        LOG << " failed";
    }

    fd_ = create_nonblocking_fd();
    int ret = connect(fd_, (struct sockaddr*)&addr, static_cast<socklen_t>(sizeof(struct sockaddr_in)));
    int r = errno;
    if (ret == 0 || r == EINPROGRESS)
    {
        // TODO
        connecting();
    }
    else
    {
        LOG << error_message() << " " << r;
        retry();
    }
}

static int socker_error(int fd)
{
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);

    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        return errno;
    }
    return optval;
}

void connector::retry()
{
    ::close(fd_);
    delay_ = delay_ + delay_ * 2;
    if (delay_ > 60)
    {
        LOG << " connect failed";
        return;
        exit(0);
    }
    LOG << "delay " << delay_ << " seconds";
    t_.set(delay_, std::bind(&connector::start, this));
}

void connector::update()
{
    context_->update(fd_, write_event, std::bind(&connector::connecd, this));
}

void connector::cancle()
{
    context_->update(fd_, none_event, std::bind(&connector::connecd, this));
}

void connector::connecd()
{
    cancle();
    int ret = socker_error(fd_);
    if (ret)
    {
        LOG << "connect failed " << fd_;
        retry();
    }
    else
    {
        LOG << "connected " << fd_;
        conn_cb_(fd_);
    }
}

void connector::connecting()
{
    LOG << "connecting " << fd_;
    update();
}
