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

struct Response
{
    uint64_t seq;
    std::string body;
    std::string session;
};

using FuncType = std::function<void(const Response&)>;

class io_context;

using TcpClientPrt = std::unique_ptr<client>;

class RtspTcpClient : noncopyable
{
private:
    struct Media
    {
        bool setup = false;  // setup command
        std::string name;    // video or audio
        std::string encoding_name;
        std::string control_path;
        std::string session_id_;
        uint16_t port;  // unuse
        uint32_t format;
        uint32_t frequency;
        std::string to_string() const
        {
            std::string str;
            std::string space(" ");
            str += name;
            str += space;
            str += encoding_name;
            str += space;
            str += control_path;
            str += space;
            str += std::to_string(port);
            str += space;
            str += std::to_string(format);
            str += space;
            str += std::to_string(frequency);
            return str;
        }
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
    std::optional<Media> parse_media(const std::string& media);
    uint64_t sequence() { return seq_; }
    // command
    void send_options(const connection_ptr& conn);
    void send_describe(const connection_ptr& conn);
    void send_setup(const connection_ptr& conn, const Media& media);
    void send_play(const connection_ptr& conn);
    // response
    void options_response(const connection_ptr& conn, const Response& response);
    void describe_response(const connection_ptr& conn, const Response& response);
    void setup_response(const connection_ptr& conn, const Response& response);
    void play_response(const connection_ptr& conn, const Response& response);
    std::string session_id();

private:
    std::string ip_;
    std::string rtsp_url_;
    uint8_t stream_id_count_ = 0;
    uint16_t port_;
    TcpClientPrt c_;
    uint64_t seq_ = 1;
    std::vector<Media> medias_;
    std::map<uint64_t, FuncType> funcs_;
};

#endif
