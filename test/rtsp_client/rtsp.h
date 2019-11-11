#ifndef __RTSP_H_
#define __RTSP_H_
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include "call_back.h"
#include "client.h"
#include "timer.h"
#include "noncopyable.h"

struct Response
{
    uint64_t seq;
    std::string body;
    std::string session;
};

class io_context;

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
};

// RtspStream --> RtpPacketArr --> H264FrameArr --> H264Stream --> file
//
using FuncType = std::function<void(const Response&)>;
using TcpClientPrt = std::unique_ptr<client>;
using RtpPacket = std::vector<uint8_t>;
using H264Frame = std::vector<uint8_t>;
using RtspStream = std::vector<uint8_t>;

using RtpPacketArr = std::vector<RtpPacket>;
using H264FrameArr = std::vector<H264Frame>;
using H264Stream = std::vector<H264Frame>;
using CallBackMap = std::map<uint64_t, FuncType>;
using MediaArr = std::vector<Media>;
using MediaMap = std::map<uint8_t, Media>;

class RtspTcpClient : noncopyable
{
private:
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
    void send_teardown(const connection_ptr& conn);
    // response
    void options_response(const connection_ptr& conn, const Response& response);
    void describe_response(const connection_ptr& conn, const Response& response);
    void setup_response(const connection_ptr& conn, const Response& response);
    void play_response(const connection_ptr& conn, const Response& response);
    void teardown_response(const connection_ptr& conn, const Response& response);
    std::string session_id();
    // pares rtsp stream date
    void push_data(const std::string& data);
    void parse_data();
    void parse_rtsp_stream();
    void parse_rtsp_command();
    //
    int stream_channel(uint8_t ch);
    //
    void parse_rtp_header();
    void parse_rtp_header(const RtpPacket &pkt);
    //
    void parse_frames_nal();
    void parse_frame_nal(const H264Frame& frame);

private:
    std::string ip_;
    std::string rtsp_url_;
    uint8_t stream_id_count_ = 0;
    uint16_t port_;
    uint64_t seq_ = 1;
    TcpClientPrt c_;
    timer t_;
    bool split_ = false;
    std::string session_id_;  // setup session
    MediaArr medias_;
    MediaMap medias_map_;  // rtp channel media
    RtspStream stream_data_;
    RtpPacketArr rtp_packets_;
    H264FrameArr h264_frames_;
    H264Frame split_frame_;
    H264Stream h264_stream_;
    CallBackMap funcs_;
};

#endif
