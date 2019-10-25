#include "call_back.h"
#include "connection.h"
#include "io_context.h"
#include "log.h"
#include "server.h"
#include <functional>
#include <iostream>

using namespace std::placeholders;

class echo_server
{
public:
    echo_server(io_context* context, int16_t port)
        : s(context, port)
    {
        s.set_on_message(std::bind(&echo_server::on_message, this, _1));
        s.set_on_connect(std::bind(&echo_server::on_connect, this, _1));
        s.set_on_close(std::bind(&echo_server::on_close, this, _1));
    }
    ~echo_server() { LOG << "echo_server over"; }
    uint16_t port() { return s.port(); }

public:
    void start() { s.start(); }
    void on_connect(const connection_ptr& conn)
    {
        LOG << conn->to_string() << " up";
    }
    void on_message(const connection_ptr& conn)
    {
        std::string buf = conn->readAll();
        std::cout << buf << std::endl;
        conn->send(buf);
    }
    void on_close(const connection_ptr& conn)
    {
        LOG << conn->to_string() << " down";
    }

private:
    server s;
};

int main(int argc, char** argv)
{
    uint16_t port = 5500;
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }

    LOG << "process pid is " << getpid();

    io_context context;

    echo_server s(&context, port);

    s.start();

    LOG << "listen port is " << s.port();
    context.run();

    return 0;
}
