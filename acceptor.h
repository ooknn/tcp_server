/*

    copyright 2019 ggyyll

*/

#ifndef ACCEPTOR_H_
#define ACCEPTOR_H_
#include <functional>
#include <memory>
#include <stdint.h>

class io_context;

class acceptor
{
    using acceptor_call_type = std::function<void(int)>;

public:
    acceptor(io_context* context, const uint16_t port);
    ~acceptor();
    void listen();
    void update();
    void set_on_connect(acceptor_call_type call) { _on_connect = call; }
    uint16_t port() { return _port; }

private:
    void bind();
    void cancle();
    void reuse_port();
    void reuse_addr();
    void accept_back(); // create connection

private:
    acceptor_call_type _on_connect;
    io_context* _context;
    bool _listening;
    uint16_t _port;
    int _fd; // listen fd
};
#endif // ACCEPTOR_H_
