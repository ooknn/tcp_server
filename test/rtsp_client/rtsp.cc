#include "rtsp.h"
#include <assert.h>
#include <algorithm>
#include <vector>
#include "client.h"
#include "connection.h"
#include "io_context.h"
#include "log.h"

const char* option_str = "OPTIONS %s RTSP/1.0\r\n"
                         "CSeq:%d\r\n"
                         "User-Agent: rtsp client(ok)"
                         "\r\n"
                         "\r\n";

const char* describe_str = "DESCRIBE %s RTSP/1.0\r\n"
                           "CSeq:%d\r\n"
                           "User-Agent: rtsp client(ok)"
                           "\r\n"
                           "\r\n";

const char* set_up_str = "SETUP %s/%s RTSP/1.0\r\n"
                         "CSeq: %d\r\n"
                         "User-Agent: rtsp client(ok)"
                         "\r\n"
                         "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n\r\n";

using StringS = std::vector<std::string>;

void split(const std::string& str, StringS* ss, const std::string& delimiters)
{
    StringS methods;
    std::string::size_type p = 0;
    std::string::size_type n = 0;
    std::string::size_type npos = std::string::npos;
    while (true)
    {
        p = str.find(delimiters, n);
        if (p == npos)
        {
            break;
        }
        n = str.find(delimiters, p + delimiters.size());
        if (n == npos)
        {
            methods.push_back(str.substr(p));
            break;
        }
        methods.push_back(str.substr(p, n - p));
    }
    std::swap(*ss, methods);
}

static void ParseRtspUrl(const std::string& url, std::string* ip, uint16_t* port)
{
    const std::string rtsp_prefix("rtsp://");
    if (url.size() < rtsp_prefix.size())
    {
        // invalid length
        return;
    }
    if (url.compare(0, rtsp_prefix.size(), rtsp_prefix))
    {
        // invalid prefix
        return;
    }
    auto sub_url = url.substr(rtsp_prefix.size());
    std::vector<std::string> v;
    while (true)
    {
        auto pos = sub_url.find_first_of('/');
        if (pos == std::string::npos)
        {
            break;
        }
        v.push_back(sub_url.substr(0, pos));
        sub_url = sub_url.substr(pos + 1);
    }
    if (v.empty())
    {
        return;
    }
    std::string ip_port = v.front();

    auto pos = ip_port.find_first_of(':');
    if (pos == std::string::npos)
    {
        return;
    }
    *ip = ip_port.substr(0, pos);
    *port = std::stoul(ip_port.substr(pos + 1));
}

RtspTcpClient::RtspTcpClient(io_context* context, const std::string& url)
    : rtsp_url_(url)
{
    ParseRtspUrl(rtsp_url_, &ip_, &port_);
    LOG << ip_ << " : " << port_;
    assert(ip_.size() > 0);
    assert(port_ > 0);
    c_ = std::make_unique<client>(context, ip_, port_);
    c_->set_on_message(
        std::bind(&RtspTcpClient::on_message, this, std::placeholders::_1));
    c_->set_on_connect(
        std::bind(&RtspTcpClient::on_connect, this, std::placeholders::_1));
    c_->set_on_close(
        std::bind(&RtspTcpClient::on_close, this, std::placeholders::_1));
}

void RtspTcpClient::connect() { c_->connect(); }

void RtspTcpClient::on_connect(const connection_ptr& conn)
{
    LOG << conn->to_string() << " UP";
    send_options(conn);
}

void RtspTcpClient::on_close(const connection_ptr& conn)
{
    LOG << conn->to_string() << " DOWN";
    c_->disconnect();
}

void RtspTcpClient::on_message(const connection_ptr& conn)
{
    std::string response_str = conn->readAll();
    LOG << response_str;
    auto response = parse_response(response_str);
    if (!response)
    {
        LOG << "parse response failed";
        return;
    }
    auto it = funcs_.find(response->seq);
    if (it == funcs_.end())
    {
        LOG << "not found function " << response->seq;
        return;
    }
    it->second(response.value());
}

void RtspTcpClient::send_options(const connection_ptr& conn)
{
    char buffer[2048 / 2] = {0};
    snprintf(buffer, sizeof buffer, option_str, rtsp_url_.data(), ++seq_);

    funcs_.emplace(sequence(), [&](const Response& response) {
        options_response(conn, response);
    });
    LOG << buffer;
    conn->send(buffer);
}

void RtspTcpClient::send_describe(const connection_ptr& conn)
{
    char buffer[2048 / 2] = {0};
    snprintf(buffer, sizeof buffer, describe_str, rtsp_url_.data(), ++seq_);
    funcs_.emplace(sequence(), [&](const Response& response) {
        describe_response(conn, response);
    });
    LOG << buffer;
    conn->send(buffer);
}

void RtspTcpClient::options_response(const connection_ptr& conn, const Response& response)
{
    assert(response.body.empty());
    send_describe(conn);
}

std::string RtspTcpClient::session_id()
{
    if (medias_.empty())
    {
        return "";
    }
    auto session = medias_[0].session_id_;
    session += "\r\n";
    return session;
}

void RtspTcpClient::send_setup(const connection_ptr& conn, const Media& media)
{
    LOG << media.to_string();

    char buffer[1024] = {0};
    uint8_t rtp_number = stream_id_count_++;
    uint8_t rtcp_number = stream_id_count_++;
    snprintf(buffer, sizeof buffer, set_up_str, rtsp_url_.data(), media.control_path.data(), ++seq_, rtp_number, rtcp_number);
    std::string setup_session(buffer);
    setup_session += session_id();

    //
    funcs_.emplace(sequence(), [&](const Response& response) {
        setup_response(conn, response);
    });

    //
    //std::string str = R"(SETUP rtsp://192.168.19.90:554/live/202441247_00000000001181000079_0_0/track0 RTSP/1.0\r\nCSeq: 4\r\nUser-Agent: /home/gyl/tools/live555/testProgs/openRTSP (LIVE555 Streaming Media v2019.05.29)\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n)";
    //LOG << str;
    LOG << buffer;
    conn->send(buffer);
}

void RtspTcpClient::setup_response(const connection_ptr& conn, const Response& response)
{
    for (auto&& media : medias_)
    {
        if (media.setup)
        {
            media.session_id_ = response.session;
            continue;
        }
        media.setup = true;
        send_setup(conn, media);
        return;
    }

    //  play command
    send_play(conn);
}

void RtspTcpClient::send_play(const connection_ptr& conn)
{
    // TODO
    // send play
}

void RtspTcpClient::play_response(const connection_ptr& conn, const Response& response)
{
    // set stream data call back
    conn->set_on_message([](const connection_ptr& conn) {
        auto data = conn->readAll();
        LOG << "recv " << data.size() << " bytes";
    });
}

void RtspTcpClient::describe_response(const connection_ptr& conn, const Response& response)
{
    assert(!response.body.empty());
    StringS medias;
    split(response.body, &medias, "m=");
    for (const auto& media : medias)
    {
        auto m = parse_media(media);
        if (m)
        {
            medias_.push_back(m.value());
        }
    }
    assert(!medias_.empty());
    auto& media = medias_[0];

    media.setup = true;
    send_setup(conn, media);
}

std::optional<RtspTcpClient::Media> RtspTcpClient::parse_media(const std::string& media)
{
    std::string a_type("a=");
    StringS attrs;
    split(media, &attrs, a_type);
    uint16_t port = 0;
    char name[64] = {0};
    char path[64] = {0};
    char codec_name[64] = {0};
    uint32_t format = 0;
    uint32_t frequency = 0;
    uint32_t channels = 0;

    if (sscanf(media.data(), "m=%s %hu RTP/AVP %u", name, &port, &format) != 3)
    {
        return {};
    }

    for (const auto& attr : attrs)
    {
        if (sscanf(attr.data(), "a=control: %s", path) == 1)
        {
            continue;
        }

        if (sscanf(attr.data(), "a=rtpmap: %u %[^/]/%u", &format, codec_name, &frequency) == 3
            || sscanf(attr.data(), "a=rtpmap: %u %[^/]/%u/%u", &format, codec_name, &frequency, &channels) == 4)
        {
            continue;
        }
    }

    RtspTcpClient::Media m;
    m.name = name;
    m.encoding_name = codec_name;
    m.control_path = path;
    m.format = format;
    m.port = port;
    m.frequency = frequency;

    return m;
}
std::optional<Response> RtspTcpClient::parse_response(
    const std::string& response)
{
    uint64_t response_seq = 0;
    std::string response_body;
    std::string session;
    {
        // prefix
        std::string prefix("RTSP/1.0 200 OK");
        if (response.compare(0, prefix.size(), prefix) != 0)
        {
            LOG << "not found 200 OK\n"
                << response;
            return {};
        }
    }
    {
        // CSeq
        std::string CSeq("CSeq: ");
        auto pos = response.find(CSeq);
        if (pos == std::string::npos)
        {
            LOG << "not found CSeq\n"
                << response;
            return {};
        }
        std::string seq = response.substr(pos + CSeq.size());
        auto it = std::find_if(seq.begin(), seq.end(), [](char ch) {
            return ch == '\n' || ch == '\r';
        });
        if (it == seq.end())
        {
            LOG << "CSeq invalid\n"
                << response;
            return {};
        }
        std::string s = seq.substr(0, it - seq.begin());
        response_seq = std::stoull(s);
        assert(response_seq);
    }
    {
        // Session
        std::string session_prefix("Session: ");
        auto pos = response.find(session_prefix);
        if (pos != std::string::npos)
        {
            std::string session_str = response.substr(pos);
            auto it = std::find_if(session_str.begin(), session_str.end(), [](char ch) {
                return ch == '\n' || ch == '\r';
            });
            if (it != session_str.end())
            {
                session = session_str.substr(0, it - session_str.begin());
            }
        }
    }
    {
        // body
        std::string content_length("Content-Length: ");
        auto pos = response.find(content_length);
        if (pos == std::string::npos)
        {
            LOG << "not found content length\n"
                << response;
            return {};
        }

        assert(pos + content_length.size() <= response.size());
        std::string sub = response.substr(pos + content_length.size());
        assert(sub.empty() == false);
        auto it = std::find_if(sub.begin(), sub.end(), [](char ch) {
            return ch == '\n' || ch == '\r';
        });
        if (it == sub.end())
        {
            LOG << "content_length invalid\n"
                << response;
            return {};
        }
        auto s = sub.substr(0, it - sub.begin());
        uint64_t length = std::stoull(s);
        if (length != 0)
        {
            it = std::find_if(sub.begin() + s.size(), sub.end(), [](char ch) { return ch != '\n' && ch != '\r'; });
            if (it == sub.end())
            {
                LOG << "response body invalid\n"
                    << response;
                return {};
            }
            response_body = sub.substr(it - sub.begin());
            assert(response_body.size() == length);
        }
    }

    return Response {response_seq, response_body, session};
}
