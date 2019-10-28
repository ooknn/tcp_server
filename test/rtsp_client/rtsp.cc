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
                         "User-Agent:rtsp client(ok)"
                         "\r\n"
                         "\r\n";

const char* describe_str = "DESCRIBE %s RTSP/1.0\r\n"
                           "CSeq:%d\r\n"
                           "User-Agent:rtsp client(ok)"
                           "\r\n"
                           "\r\n";
const char* set_up_str = "SETUP rtsp://%s:%d/%s/%s RTSP/1.0\r\n"
                         "CSeq: %d\r\n"
                         "User-Agent: %s\r\n"
                         "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n"
                         "\r\n";

//"rtsp://[<username>[:<password>]@]<server-address-or-name>[:<port>][/<stream-name>]"
// rtsp://192.168.19.90:554/live/202389821_33010800001311000026_0_0
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

void RtspTcpClient::send_options(const connection_ptr& conn)
{
    char buffer[2048 / 2] = {0};
    snprintf(buffer, sizeof buffer, option_str, rtsp_url_.data(), ++seq_);

    funcs_.emplace(sequence(), [&](const std::string& response) {
        assert(response.empty());
    });
    LOG << buffer;
    conn->send(buffer);
}

void RtspTcpClient::send_describe(const connection_ptr& conn)
{
    char buffer[2048 / 2] = {0};
    snprintf(buffer, sizeof buffer, describe_str, rtsp_url_.data(), ++seq_);

    LOG << buffer;
    conn->send(buffer);
}

void RtspTcpClient::options_response(const std::string& response)
{
    assert(response.empty());
}
void RtspTcpClient::describe_response(const std::string& response)
{
    assert(!response.empty());
    /*
        m=video 0 RTP/AVP 97
         a=rtpmap:97 H264/90000
        a=fmtp:97
        a=control:track0
        m=audio 0 RTP/AVP 8
        a=rtpmap:8 PCMA/8000/1
        a=control:track1
    */
    std::string str_copy = response;
    while (true)
    {
        std::string media_prefix("m=");
        auto pos = str_copy.find(media_prefix);
        if (pos == std::string::npos)
        {
            break;
        }
    }
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
    it->second(response->body);
}

std::optional<RtspTcpClient::Response> RtspTcpClient::parse_response(
    const std::string& response)
{
    uint64_t response_seq = 0;
    std::string response_body;
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
        if (length == 0)
        {
            LOG << "response body empty";
            return {};
        }
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
    auto it = funcs_.find(response_seq);
    if (it == funcs_.end())
    {
        LOG << "response seq invalid\n"
            << response_seq << "\n"
            << response;
        return {};
    }

    return RtspTcpClient::Response {response_seq, response_body};
}
