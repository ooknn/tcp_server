#ifndef __SERVER_H__
#define __SERVER_H__

#include <set>
#include "call_back.h"
class acceptor;
class io_context;

class server
{
    using accept_ptr = std::unique_ptr<acceptor>;

public:
    server(io_context* context, uint16_t port);
    ~server();
    void start();
    void set_on_close(close_call_back call) { _on_close = call; }
    void set_on_message(message_call_back call) { _on_message = call; }
    void set_on_connect(connect_call_back call) { _on_connect = call; }
    uint16_t port();

private:
    void on_close(connection_ptr conn);
    void on_message(connection_ptr conn);
    void on_connect(int fd);

private:
    io_context* _context;
    accept_ptr _apt;
    std::set<connection_ptr> _conns;
    close_call_back _on_close;
    message_call_back _on_message;
    connect_call_back _on_connect;
};

#endif
