#include "rtsp.h"
#include <assert.h>
#include <algorithm>
#include <vector>
#include "client.h"
#include "connection.h"
#include "io_context.h"
#include "log.h"
#include <chrono>
#include <thread>
#include <deque>

const static int VIDEO_RTP_CHANNEL = 0;
const static int VIDEO_RTCP_CHANNEL = 1;
const static int AUDIO_RTP_CHANNEL = 2;
const static int AUDIO_RTCP_CHANNEL = 3;

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

const char* setup_str = "SETUP %s/%s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"User-Agent: rtsp client(ok)"
"\r\n"
"Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n";

const char* play_str = "PLAY %s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"User-Agent: rtsp clinet(ok)\r\n"
"%s"
"Range: npt=0.000-\r\n"
"\r\n";

const char* teardown_str = "TEARDOWN %s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"User-Agent: rtsp clinet(ok)\r\n"
"%s"
"\r\n";

using StringS = std::vector<std::string>;

static void split(const std::string& str, StringS* ss, const std::string& delimiters)
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
      , t_(context)
{
    ParseRtspUrl(rtsp_url_, &ip_, &port_);
    LOG << ip_ << " : " << port_;
    assert(ip_.size() > 0);
    assert(port_ > 0);
    c_ = std::make_unique<client>(context, ip_, port_);
    //c_->set_on_message([&](const connection_ptr&conn){ on_message(conn); });
    c_->set_on_message( std::bind(&RtspTcpClient::on_message, this, std::placeholders::_1));
    c_->set_on_connect( std::bind(&RtspTcpClient::on_connect, this, std::placeholders::_1));
    c_->set_on_close( std::bind(&RtspTcpClient::on_close, this, std::placeholders::_1));
    //
    decoder_ = std::make_shared<H264::H264Decode>();
    assert(decoder_->Valid());
}

RtspTcpClient::~RtspTcpClient()
{
    decode_h264_frame();
    decoder_->FlushDecode();
    auto yuvs = decoder_->ReceiveAllYuvFrame();
    if(!yuvs.empty())
    {
        yuv_stream_.insert(yuv_stream_.end(),yuvs.begin(),yuvs.end());
    }
    LOG << "begin write yuv frmae to file  yuv frame size " << yuv_stream_.size();
    std::string filename = std::to_string(time(NULL));
    filename += ".yuv";
    FILE* fp = fopen(filename.data(),"ab");
    assert(fp);
    for(const auto&yuv:yuv_stream_)
    {
        fwrite(yuv.data(),1,yuv.size(),fp);
    }
    fclose(fp);
    LOG << "end write file : " << filename;
}

void RtspTcpClient::connect() { c_->connect(); }

void RtspTcpClient::on_connect(const connection_ptr& conn)
{
    LOG << conn->to_string() << " UP";
    send_options(conn);
}

void RtspTcpClient::on_close(const connection_ptr& conn)
{
    LOG << conn->to_string() << " DOWN\n";
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
    funcs_.emplace(sequence(), [&](const Response& response) { options_response(conn, response); });
    LOG << buffer;
    conn->send(buffer);
}

void RtspTcpClient::send_describe(const connection_ptr& conn)
{
    char buffer[2048 / 2] = {0};
    snprintf(buffer, sizeof buffer, describe_str, rtsp_url_.data(), ++seq_);
    funcs_.emplace(sequence(), [&](const Response& response) { describe_response(conn, response); });
    LOG << buffer;
    conn->send(buffer);
}

void RtspTcpClient::options_response(const connection_ptr& conn, const Response& response)
{
    assert(response.body.empty());
    send_describe(conn);
}



void RtspTcpClient::send_setup(const connection_ptr& conn, const Media& media)
{
    char buffer[1024] = {0};
    uint8_t rtp_number = stream_id_count_++;
    uint8_t rtcp_number = stream_id_count_++;
    medias_map_.insert(std::make_pair(rtp_number, media));
    snprintf(buffer, sizeof buffer, setup_str, rtsp_url_.data(), media.control_path.data(), ++seq_, rtp_number, rtcp_number);

    std::string setup_session(buffer);
    setup_session += session_id();
    setup_session += "\r\n";
    //
    funcs_.emplace(sequence(), [&](const Response& response) { setup_response(conn, response); });

    LOG << setup_session;
    conn->send(setup_session);
}

void RtspTcpClient::setup_response(const connection_ptr& conn, const Response& response)
{
    for (auto&& media : medias_)
    {
        if (session_id_.empty())
        {
            session_id_ = response.session;
        }
        if (media.setup)
        {
            continue;
        }
        media.setup = true;
        send_setup(conn, media);
        return;
    }

    //  play command
    send_play(conn);
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

void RtspTcpClient::send_play(const connection_ptr& conn)
{
    // send play
    char buffer[1024] = {0};
    snprintf(buffer, sizeof buffer, play_str, rtsp_url_.data(), ++seq_, session_id().data());
    funcs_.emplace(sequence(), [&](const Response& response) { play_response(conn, response); });
    conn->set_on_message([&](const connection_ptr& conn) { auto data = conn->readAll(); push_data(data); parse_data(); });
    LOG << buffer;
    conn->send(buffer);
}

void RtspTcpClient::play_response([[maybe_unused]]const connection_ptr& conn,[[maybe_unused]] const Response& response)
{
    LOG << "Start receive 13 second stream ... ";
    t_.set(13, [&]() { send_teardown(conn); });
}

void RtspTcpClient::send_teardown(const connection_ptr& conn)
{
    char buffer[1024] = {0};
    snprintf(buffer, sizeof buffer, teardown_str, rtsp_url_.data(), ++seq_, session_id().data());
    funcs_.emplace(sequence(), [&](const Response& response) { teardown_response(conn, response); });
    conn->set_on_message([&](const connection_ptr& conn) { std::string tear_str = conn->readAll(); LOG << tear_str; conn->close(); });
    LOG << buffer;
    conn->send(buffer);
}

void RtspTcpClient::teardown_response([[maybe_unused]]const connection_ptr& conn,[[maybe_unused]] const Response& response)
{
    LOG << "recv teardown response";
}

void RtspTcpClient::parse_data()
{
    if (stream_data_.empty())
    {
        return;
    }
    parse_rtsp_command();
    parse_rtsp_stream();
    parse_rtp_header();
    parse_frames_nal();
    decode_h264_frame();
}

std::string RtspTcpClient::session_id()
{
    if (session_id_.empty())
    {
        return "";
    }
    auto session = session_id_;
    session += "\r\n";
    return session;
}

void RtspTcpClient::push_data(const std::string& data)
{
    stream_data_.insert(stream_data_.end(), data.begin(), data.end());
}

int RtspTcpClient::stream_channel(uint8_t ch)
{
    return ch;
}

void RtspTcpClient::parse_rtp_header()
{
    for (auto&& pkt : rtp_packets_)
    {
        parse_rtp_header(pkt);
    }
    rtp_packets_.clear();
}

void RtspTcpClient::parse_frames_nal()
{
    for (const auto& frame : h264_frames_)
    {
        parse_frame_nal(frame);
    }
    h264_frames_.clear();
}

void RtspTcpClient::decode_h264_frame()
{
    for(const H264::H264Frame &f : h264_stream_)
    {
        decoder_->SendH264Frame(f);
    }
    h264_stream_.clear();
    auto yuvs = decoder_->ReceiveAllYuvFrame();
    if(!yuvs.empty())
    {
        yuv_stream_.insert(yuv_stream_.end(),yuvs.begin(),yuvs.end());
    }
}

std::optional<Response> RtspTcpClient::parse_response(const std::string& response)
{
    uint64_t response_seq = 0;
    std::string response_body;
    std::string session;
    // prefix
    std::string prefix("RTSP/1.0 200 OK");
    if (response.compare(0, prefix.size(), prefix) != 0)
    {
        LOG << "not found 200 OK\n" << response;
        return {};
    }

    response_seq = rtsp_command_seq(response) ;
    session = rtsp_command_session(response);
    response_body = rtsp_command_body(response);

    return Response {response_seq, response_body, session};
}


std::optional<Media> RtspTcpClient::parse_media(const std::string& media)
{
    std::string a_type("a=");
    StringS attrs;
    split(media, &attrs, a_type);
    char name[64] = {0};
    char path[64] = {0};
    char codec_name[64] = {0};
    uint32_t channels = 0;
    Media m;
    if (sscanf(media.data(), "m=%s %hu RTP/AVP %u", name, &m.port, &m.format) != 3)
    {
        assert(false);
        return {};
    }

    for (const auto& attr : attrs)
    {
        if (sscanf(attr.data(), "a=control: %s", path) == 1)
        {
            continue;
        }

        if (sscanf(attr.data(), "a=rtpmap: %u %[^/]/%u", &m.format, codec_name, &m.frequency) == 3
                || sscanf(attr.data(), "a=rtpmap: %u %[^/]/%u/%u", &m.format, codec_name, &m.frequency, &channels) == 4)
        {
            continue;
        }
    }

    m.name = name;
    m.encoding_name = codec_name;
    m.control_path = path;

    return m;
}

uint32_t RtspTcpClient::rtsp_command_seq(const std::string&response)
{
    // CSeq
    std::string CSeq("CSeq: ");
    auto pos = response.find(CSeq);
    if (pos == std::string::npos)
    {
        LOG << "not found CSeq\n" << response;
        return {};
    }
    std::string seq = response.substr(pos + CSeq.size());
    auto it = std::find_if(seq.begin(), seq.end(), [](char ch) { return ch == '\n' || ch == '\r'; });
    if (it == seq.end())
    {
        LOG << "CSeq invalid\n" << response;
        return {};
    }
    std::string s = seq.substr(0, it - seq.begin());
    uint32_t response_seq = std::stoull(s);
    assert(response_seq);
    return response_seq;
}

std::string RtspTcpClient::rtsp_command_session(const std::string&response)
{
    // Session
    std::string session_prefix("Session: ");
    auto pos = response.find(session_prefix);
    if (pos == std::string::npos)
    {
        return {};
    }

    std::string session_str = response.substr(pos);
    auto it = std::find_if(session_str.begin(), session_str.end(), [](char ch) { return ch == '\n' || ch == '\r'; });
    if (it == session_str.end())
    {
        return {};
    }

    {
        std::string session = session_str.substr(0, it - session_str.begin());
        auto it = session.find(';');
        if (it != std::string::npos)
        {
            session = session.substr(0, it);
        }
        return session;
    }
    return {};
}

std::string RtspTcpClient::rtsp_command_body(const std::string&response)
{
    // body
    std::string content_length("Content-Length: ");
    auto pos = response.find(content_length);
    if (pos == std::string::npos)
    {
        return {};
    }
    assert(pos + content_length.size() <= response.size());
    std::string sub = response.substr(pos + content_length.size());
    assert(sub.empty() == false);
    auto it = std::find_if(sub.begin(), sub.end(), [](char ch) { return ch == '\n' || ch == '\r'; });
    if (it == sub.end())
    {
        LOG << "content_length invalid\n" << response;
        return {};
    }
    auto s = sub.substr(0, it - sub.begin());
    uint64_t length = std::stoull(s);
    if (length == 0)
    {
        return {};
    }

    it = std::find_if(sub.begin() + s.size(), sub.end(), [](char ch) { return ch != '\n' && ch != '\r'; });
    if (it == sub.end())
    {
        LOG << "response body invalid\n" << response;
        return {};
    }
    std::string response_body = sub.substr(it - sub.begin());
    assert(response_body.size() == length);
    return response_body;
}

void RtspTcpClient::parse_rtsp_stream()
{
    while (true)
    {
        if (stream_data_.size() < 4)
        {
            // 1 byte $ , 1 byte channel_id,  2 byte size
            return;
        }
        size_t i = 0;
        if (stream_data_[i] != '$')
        {
            return;
        }

        uint64_t stream_size = stream_data_.size();
        // https://www.ietf.org/rfc/rfc2326.txt : 10.12
        int channel_id = stream_channel(stream_data_[++i]);
        assert(channel_id < 4);
        uint8_t size1 = stream_data_[++i];
        uint8_t size2 = stream_data_[++i];
        assert(i == 3);
        // payload size
        size_t size = (size1 << 8) | size2;
        if (stream_size - 4 < size)
        {
            //LOG << "Not a complete packet ";
            return;
        }
        auto begin = stream_data_.begin();
        // begin + size + 4 [1 byte $ 1 byte channel_id 2 byte size]
        auto end = begin + size + 4;
        std::vector<uint8_t> frame(begin + 4, begin + 4 + size);
        assert(frame.size() == size);
        assert(frame.size() <= stream_size - 4);
        stream_data_.erase(begin, end);
        if (channel_id == VIDEO_RTCP_CHANNEL || channel_id == AUDIO_RTCP_CHANNEL)
        {
            // rtcp stream
            continue;
        }

        // check media
        auto it = medias_map_.find(channel_id);
        assert(it != medias_map_.end());
        auto media = it->second;

        rtp_packets_.push_back(frame);
    }
    return;
}


void RtspTcpClient::parse_rtsp_command()
{
    if (stream_data_.empty())
    {
        return;
    }
    uint8_t ch = stream_data_[0];
    if (ch == '$')
    {
        return;
    }
    if (ch == 0XFE || ch == 0XFF)
    {
        // 0xFF and 0xFE are used as special signaling characters
        std::copy(stream_data_.begin() + 1, stream_data_.end(), stream_data_.begin());
        return;
    }
    assert(ch != '$');
    std::string command;
    for (const auto& ch : stream_data_)
    {
        if (ch == '$')
        {
            break;
        }

        command.push_back(ch);
    }

    if (command.empty())
    {
        return;
    }
    assert(command.size() <= stream_data_.size());
    auto begin = stream_data_.begin();
    auto end = begin + command.size();
    stream_data_.erase(begin, end);
    // check command end \r\n\r\n

    auto response = parse_response(command);
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
    LOG << command;
    it->second(response.value());
}



void RtspTcpClient::parse_rtp_header(const RtpPacket& pkt)
{

    if (pkt.size() < 12)
    {
        return;
    }
    const uint8_t* ptr = pkt.data();
    uint32_t frame_size = pkt.size();
    uint32_t rtpHdr = ntohl(*(uint32_t*)(ptr));
    ptr += 4;
    frame_size -= 4;
    [[maybe_unused]]bool rtpMarkerBit = (rtpHdr & 0x00800000) != 0;
    [[maybe_unused]]uint32_t rtpTimestamp = ntohl(*(uint32_t*)(ptr));
    ptr += 4;
    frame_size -= 4;
    [[maybe_unused]]uint32_t rtpSSRC = ntohl(*(uint32_t*)(ptr));
    ptr += 4;
    frame_size -= 4;
    // Check the RTP version number (it should be 2):
    if ((rtpHdr & 0xC0000000) != 0x80000000)
    {
        assert(false);
        return;
    }
    [[maybe_unused]]uint8_t rtpPayloadType = (uint8_t)((rtpHdr & 0x007F0000) >> 16);
    // Skip over any CSRC identifiers in the header:
    unsigned cc = (rtpHdr >> 24) & 0x0F;
    if (frame_size < cc * 4)
    {
        assert(false);
        return;
    }

    ptr += cc * 4;
    frame_size -= cc * 4;

    // Check for (& ignore) any RTP header extension
    if (rtpHdr & 0x10000000)
    {
        if (frame_size < 4)
        {
            assert(false);
            return;
        }
        unsigned extHdr = ntohl(*(u_int32_t*)(ptr));
        ptr += 4;
        frame_size -= 4;
        unsigned remExtSize = 4 * (extHdr & 0xFFFF);
        if (frame_size < remExtSize)
        {
            assert(false);
            return;
        }
        ptr += remExtSize;
    }
    // Discard any padding bytes:
    uint16_t numPaddingBytes = 0;
    if (rtpHdr & 0x20000000)
    {
        if (frame_size == 0)
        {
            assert(false);
            return;
        }
        numPaddingBytes = (uint16_t)((ptr)[frame_size - 1]);
        if (frame_size < numPaddingBytes)
        {
            assert(false);
            return;
        }
    }
    [[maybe_unused]] uint16_t rtpSeqNo = (uint16_t)(rtpHdr & 0xFFFF);

    uint32_t skip = ptr - pkt.data();
    h264_frames_.push_back({pkt.begin() + skip, pkt.end() - numPaddingBytes});
}


void RtspTcpClient::parse_frame_nal(const H264::H264Frame& frame)
{
    // (Nal Unit Type) reference : https://tools.ietf.org/html/rfc6184 and https://tools.ietf.org/html/rfc3984
    if (frame.size() < 1)
    {
        return;
    }
    uint8_t nal_type = (frame[0] & 0x1F);
    switch (nal_type)
    {
        case 5:
        case 7:
        case 8:
            {
                parse_sps_idr_pps_nal(frame);
                break;
            }
        case 28:
        case 29:
            {
                parse_split_nal(frame);
                break;
            }
        default:
            {
                // see rfc
                if (nal_type < 20)
                {
                    parse_other_nal(frame);
                }
                break;
            }
    }
}

void RtspTcpClient::parse_other_nal(const H264::H264Frame& frame)
{
    if (idr_)
    {
        h264_stream_.push_back(frame);
    }
}

void RtspTcpClient::parse_sps_idr_pps_nal(const H264::H264Frame& frame)
{
    idr_ = true;
    h264_stream_.push_back(frame);
}

void RtspTcpClient::parse_split_nal(const H264::H264Frame& frame)
{
    assert(frame.size() > 2);

    unsigned char startBit = frame[1] & 0x80;
    unsigned char endBit = frame[1] & 0x40;

    if (startBit)
    {
        assert(split_ == false);
        split_ = true;
        uint8_t byte = (frame[0] & 0xE0) | (frame[1] & 0x1F);
        split_frame_.push_back(byte);
        split_frame_.insert(split_frame_.end(), frame.begin() + 2, frame.end());
    }
    if (endBit)
    {
        assert(split_ == true);
        split_ = false;
        split_frame_.insert(split_frame_.end(), frame.begin() + 2, frame.end());
        if (idr_)
        {
            h264_stream_.push_back(split_frame_);
        }
        split_frame_.clear();
    }
    if (endBit == 0 && startBit == 0)
    {
        assert(split_ == true);
        split_frame_.insert(split_frame_.end(), frame.begin() + 2, frame.end());
    }
}

