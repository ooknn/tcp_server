#include "client.h"
#include "connector.h"
#include "connection.h"
#include "io_context.h"
#include "log.h"

void client::on_connect(int fd)
{
    LOG << " " << fd;
    conn_ = std::make_shared<connection>(context_, fd);
    conn_->set_on_message(on_message_);
    conn_->set_on_close(std::bind(&client::on_close, this, std::placeholders::_1));
    on_conn_(conn_);
}

void client::connect()
{
    connector_.set_on_connect(std::bind(&client::on_connect, this, std::placeholders::_1));
    connector_.start();
}

void client::disconnect()
{
    if (conn_)
    {
        conn_->close();
    }
}

void client::on_close(const connection_ptr& conn)
{
    on_close_(conn);
}
