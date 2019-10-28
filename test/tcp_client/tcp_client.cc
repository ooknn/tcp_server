#include <string>
#include "client.h"
#include "io_context.h"
#include "noncopyable.h"
#include "connection.h"
#include "log.h"

class TcpClient : noncopyable
{
public:
    TcpClient(io_context* context, const std::string& ip, uint16_t port)
        : c(context, ip, port)
    {

        c.set_on_message(std::bind(&TcpClient::on_message, this, std::placeholders::_1));
        c.set_on_connect(std::bind(&TcpClient::on_connect, this, std::placeholders::_1));
        c.set_on_close(std::bind(&TcpClient::on_close, this, std::placeholders::_1));
    }

    ~TcpClient() = default;

    void connect()
    {
        c.connect();
    }
    void on_connect(const connection_ptr& conn)
    {
        LOG << conn->to_string() << " UP";
        conn->send("hello world\n");
    }
    void on_close(const connection_ptr& conn)
    {
        LOG << conn->to_string() << " DOWN";
        c.disconnect();
    }
    void on_message(const connection_ptr& conn)
    {
        LOG << conn->readAll();
        conn->send("hello world\n");
    }

private:
    client c;
};

int main(int argc, char** argv)
{
    uint16_t port = 5500;
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }
    io_context context;
    std::string ip("127.0.0.1");

    LOG << ip << " : " << port;

    TcpClient c(&context, ip, port);

    c.connect();

    context.run();
    return 0;
}
