#include "server.h"
#include "acceptor.h"
#include "connection.h"

server::server(io_context* context, uint16_t port)
    : _context(context)
    , _apt(std::make_unique<acceptor>(context, port))
{
    _apt->set_on_connect( std::bind(&server::on_connect, this, std::placeholders::_1));
}

server::~server() {}

void server::start()
{
    _apt->listen();
    _apt->update();
}
uint16_t server::port() { return _apt->port(); }

void server::on_close(connection_ptr conn)
{
    _on_close(conn);
    _conns.erase(conn);
    conn->close();
}

void server::on_message(connection_ptr conn)
{
    _on_message(conn);
}

void server::on_connect(int fd)
{
    if (fd == 0)
    {
        return;
    }
    auto conn = std::make_shared<connection>(_context, fd);

    conn->set_on_message( std::bind(&server::on_message, this, std::placeholders::_1));
    conn->set_on_close(std::bind(&server::on_close, this, std::placeholders::_1));

    _conns.insert(conn);
    _on_connect(conn);
}
