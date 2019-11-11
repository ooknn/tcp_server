#include "connection.h"
#include "error.h"
#include "io_context.h"
#include "log.h"
#include <assert.h>

connection::connection(io_context* context, int fd)
    : _fd(fd)
    , _context(context)
{
    input_buffer.resize(kInitSize);
    output_buffer.reserve(kInitSize);
    input_index_ = 0;
    output_index_ = 0;
    local_address();
    peer_address();
    enable_read();
    _closed = false;
}
// bug ?
connection::~connection()
{
    if (!_closed)
    {
        cancle();
    }
    ::close(_fd);
}

std::string connection::to_string()
{
    char buf[64] = {0};
    size_t size = sizeof buf;
    ::inet_ntop(AF_INET, &peer_addr_.sin_addr, buf, static_cast<socklen_t>(size));
    uint16_t port = be16toh(peer_addr_.sin_port);
    std::string tcp(buf);
    tcp += " : ";
    tcp += std::to_string(port);
    return tcp;
}

void connection::peer_address()
{
    memset(&peer_addr_, 0, sizeof peer_addr_);
    socklen_t addrlen = static_cast<socklen_t>(sizeof peer_addr_);
    if (::getpeername(_fd, static_cast<struct sockaddr*>(static_cast<void*>(&peer_addr_)), &addrlen) < 0)
    {
        LOG << " failed";
    }
}
void connection::local_address()
{
    memset(&local_addr_, 0, sizeof local_addr_);
    socklen_t addrlen = static_cast<socklen_t>(sizeof local_addr_);
    if (::getsockname(_fd, static_cast<struct sockaddr*>(static_cast<void*>(&local_addr_)), &addrlen)
        < 0)
    {
        LOG << " failed";
    }
}
void connection::update()
{
    _context->update(_fd, _ev, std::bind(&connection::call_back, this));
}

void connection::call_back()
{
    if (is_write())
    {
        if (output_buffer.empty() == false)
        {
            write_buf();
        }
    }
    if (is_read())
    {
        ssize_t n = readv_buff();
        if (n == 0)
        {
            _on_close(shared_from_this());
            //
            close();
        }
        else if (n > 0)
        {
            _on_message(shared_from_this());
        }
        else
        {
            LOG << error_message();
        }
    }
    return;
}

ssize_t connection::readv_buff()
{
    ssize_t total_size = 0;
    while (true)
    {
        char extrabuf1[65536] = {0};
        char extrabuf2[65536] = {0};
        size_t extrabuf_len = sizeof extrabuf1;
        struct iovec vec[2];
        vec[0].iov_base = extrabuf1;
        vec[0].iov_len = extrabuf_len;
        vec[1].iov_base = extrabuf2;
        vec[1].iov_len = extrabuf_len;
        const int iovcnt = 2;
        const ssize_t n = ::readv(_fd, vec, iovcnt);
        if (n <= 0)
        {
            if (errno == EAGAIN)
            {
                break;
            }
            return n;
        }
        size_t space = input_buffer.size() - input_index_;
        if (space < static_cast<size_t>(n))
        {
            input_buffer.resize(input_index_ + n);
        }
        else
        {
            // narrow space
            if (space > static_cast<size_t>(2 * input_index_))
            {
                input_buffer.resize(2 * input_index_);
            }
        }
        if (static_cast<size_t>(n) >= extrabuf_len)
        {
            assert(input_buffer.size() - input_index_ >= static_cast<size_t>(n));
            size_t buf1_len = extrabuf_len;
            size_t buf2_len = n - buf1_len;
            std::copy(extrabuf1, extrabuf1 + buf1_len, input_buffer.begin() + input_index_);
            input_index_ += buf1_len;
            std::copy(extrabuf2, extrabuf2 + buf2_len, input_buffer.begin() + input_index_);
            input_index_ += buf2_len;
        }
        else
        {
            std::copy(extrabuf1, extrabuf1 + n, input_buffer.begin() + input_index_);
            input_index_ += n;
        }

        total_size += n;
        //LOG << "readv size " << n << " total_size " << total_size
            //<< " input_buffer size " << input_buffer.size() << " input_index_ " << input_index_;
    }
    return total_size;
}

void connection::cancle()
{
    _ev = none_event;
    update();
}

void connection::shutdown()
{
    if (_closed)
    {
        return;
    }
    cancle();
    if (::shutdown(_fd, SHUT_WR) < 0)  // close(_fd);  //Linux 3.7
    {
        LOG << "shot down : " << error_message();
    }

    _closed = true;
}

void connection::close()
{
    if (_closed)
    {
        return;
    }
    cancle();
    if (::close(_fd) < 0)  // close(_fd);  //Linux 3.7
    {
        LOG << "shot down : " << error_message();
    }

    _closed = true;
}

void connection::write_buf()
{
    if (output_buffer.empty())
    {
        return;
    }
    ssize_t length = output_buffer.size();
    ssize_t n = ::write(_fd, output_buffer.data(), length);
    if (n > 0)
    {
        if (n == length)
        {
            output_buffer.clear();
        }
        else
        {
            std::copy(output_buffer.begin() + n, output_buffer.end(), output_buffer.begin());
        }
        output_index_ -= n;
    }

    //LOG << "write length is : " << n << " output_buffer size " << output_buffer.size() << " ev " << _ev;

    if (is_write() == false)
    {
        if (output_buffer.empty() == false)
        {
            enable_write();
        }
    }
    if (is_write())
    {
        if (output_buffer.empty())
        {
            disenable_write();
        }
    }
}

void connection::send(const std::string& message)
{
    if (_closed)
    {
        return;
    }
    size_t s = message.size();
    if (output_buffer.size() - output_index_ < s)
    {
        output_buffer.resize(s + output_index_);
    }
    std::copy(message.begin(), message.end(), output_buffer.data() + output_index_);
    output_index_ += s;
    write_buf();
}

std::string connection::readNByte(int n)
{
    if (n < 0)
    {
        return "";
    }

    assert(input_index_ > static_cast<ssize_t>(n));
    std::string str(input_buffer.begin(), input_buffer.begin() + n);
    std::copy(input_buffer.begin() + n, input_buffer.end(), input_buffer.begin());
    input_index_ -= n;
    return str;
}

std::string connection::readAll()
{
    std::string str(input_buffer.begin(), input_buffer.begin() + input_index_);
    input_buffer.clear();
    input_index_ = 0;
    return str;
}
