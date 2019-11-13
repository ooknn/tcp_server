#include "decoder.h"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
};
#include <assert.h>



#define INBUF_SIZE 4096

using namespace H264;

H264Decode::H264Decode()
{
    valid = InitMember();
}
H264Decode::~H264Decode()
{
    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
}
void H264Decode::SendH264Frame(const H264Frame& f)
{
    static const unsigned char  start_code[4] = {0x00, 0x00, 0x00, 0x01};
    H264Frame tmp;
    tmp.insert(tmp.end(),std::begin(start_code),std::end(start_code));
    tmp.insert(tmp.end(),std::begin(f),std::end(f));
    size_t data_size = tmp.size();
    uint8_t* data = tmp.data();
    while (data_size > 0)
    {
        int ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size, data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0)
        {
            LOG << "Error while parsing";
            assert(false);
            //avcodec_flush_buffers(c);
            return;
        }
        data += ret;
        data_size -= ret;

        if (pkt->size)
        {
            Decode(pkt);
        }
    }
}

void H264Decode::ReceiveYuvFrame(YuvFrame*f)
{
    if(stream_.empty())
    {
        return;
    }
    *f = stream_.front();
    stream_.pop_front();
}
YuvStream H264Decode::ReceiveAllYuvFrame()
{
    if(stream_.empty())
    {
        return YuvStream{};
    }
    YuvStream yuvs {stream_.begin(),stream_.end()} ;
    stream_.clear();
    return yuvs;
}
void H264Decode::FlushDecode()
{
    Decode(NULL);
};


void H264Decode::Reset()
{
    avcodec_flush_buffers(c);
}

void H264Decode::Decode(AVPacket *p)
{
    int ret = avcodec_send_packet(c, p);
    if (ret < 0)
    {
        //LOG << "Error sending a packet for decoding";
        return;
    }
    while (ret >= 0)
    {
        ret = avcodec_receive_frame(c, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            //LOG << " EAGAIN or AVERROR_EOF";
            return;
        }
        else if (ret < 0)
        {
            // Reset ?
            return;
        }

        YuvFrame yuv;
        uint32_t size   = c->width * c->height;
        auto y_pos = 0;
        auto u_pos = y_pos + size;
        auto v_pos = u_pos + size / 4;

        yuv.resize(size * 3 / 2);
        std::copy(frame->data[0],frame->data[0] + size,yuv.begin() + y_pos);
        std::copy(frame->data[1],frame->data[1] + (size/4),yuv.begin() + u_pos);
        std::copy(frame->data[2],frame->data[2] + (size/4),yuv.begin() + v_pos);
        stream_.push_back(yuv);
    }

}
bool H264Decode::InitMember()
{
#define Point_Address(x) static_cast<const void*>(x)

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        LOG << " failed  " << Point_Address(codec);
        return false;
    }

    parser = av_parser_init(codec->id);
    c = avcodec_alloc_context3(codec);
    if (!parser || !c)
    {
        LOG << "failed " << Point_Address( parser )<< " " << Point_Address( c );
        return false;
    }

    if (avcodec_open2(c, codec, NULL) < 0)
    {
        LOG << "Could not open codec";
        return false;
    }

    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if (!frame || !pkt)
    {
        LOG << "Could not allocate  frame or packet ";
        return false;
    }
    return true;
}

