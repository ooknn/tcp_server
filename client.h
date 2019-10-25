#ifndef CLIENT_H_
#define CLIENT_H_

#include <string>
#include "call_back.h"
#include "connector.h"
#include "noncopyable.h"

class io_context;

class client : noncopyable
{
public:
    client(io_context* context, const std::string& ip, uint16_t port)
        : context_(context)
        , connector_(context, ip, port) {};
    ~client() = default;
    void set_on_connect(connect_call_back cb) { on_conn_ = cb; };
    void set_on_message(message_call_back cb) { on_message_ = cb; };
    void set_on_close(close_call_back cb) { on_close_ = cb; };
    void connect();
    void disconnect();

private:
    void on_connect(int fd);
    //void on_message(const connection_ptr& conn);
    void on_close(const connection_ptr& conn);
    io_context* context_;
    connector connector_;
    connection_ptr conn_;
    connect_call_back on_conn_;
    message_call_back on_message_;
    close_call_back on_close_;
};

#endif
