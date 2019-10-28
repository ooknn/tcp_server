#ifndef __RTSP_H_
#define __RTSP_H_
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include "call_back.h"
#include "client.h"
#include "noncopyable.h"

class io_context;

using TcpClientPrt = std::unique_ptr<client>;
using FuncType = std::function<void(const std::string&)>;

class RtspTcpClient : noncopyable
{
private:
    std::map<uint64_t, FuncType> funcs_;
    struct Response
    {
        uint64_t seq;
        std::string body;
    };

    // m=<media> <port>/<number of ports> <proto> <fmt> ...

    // a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding
    // parameters>]

    struct Media
    {
        std::string name;  // video or audio
        std::string proto;
        uint32_t rtpmap;
        std::string encoding_name;
        uint32_t rate;
        uint32_t parameters;
    };

public:
    RtspTcpClient(io_context* context, const std::string& url);
    ~RtspTcpClient() = default;

    void connect();
    void on_connect(const connection_ptr& conn);
    void on_close(const connection_ptr& conn);
    void on_message(const connection_ptr& conn);

private:
    std::optional<Response> parse_response(const std::string& response);
    uint64_t sequence() { return seq_; }
    // command
    void send_options(const connection_ptr& conn);
    void send_describe(const connection_ptr& conn);
    // response
    void options_response(const std::string& response);
    void describe_response(const std::string& response);

private:
    std::string ip_;
    std::string rtsp_url_;
    uint16_t port_;
    TcpClientPrt c_;
    uint64_t seq_ = 1;
};

#endif
