#ifndef CONNECTOR_H_
#define CONNECTOR_H_

#include <string>
#include <memory>
#include "noncopyable.h"
#include "call_back.h"
#include "timer.h"

class io_context;

class connector : noncopyable, std::enable_shared_from_this<connector>
{
    using connect_call_back = std::function<void(int)>;

public:
    connector(io_context* context, const std::string& ip, uint16_t port);
    ~connector();
    void set_on_connect(connect_call_back cb) { conn_cb_ = cb; };
    const std::string local_ip();
    uint16_t local_port();
    const std::string peer_ip();
    uint16_t peer_port();
    void update();
    void cancle();
    void connecd();
    void start();
    void retry();
    void connecting();

protected:
    int fd_;
    std::string ip_;
    uint16_t port_;
    uint64_t delay_;
    io_context* context_;
    timer t_;
    connect_call_back conn_cb_;
};

#endif
