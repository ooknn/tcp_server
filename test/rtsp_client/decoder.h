#ifndef __DECODER_H__
#define __DECODER_H__

#include "noncopyable.h"
#include "log.h"
#include <deque>
#include <vector>

struct    AVCodec;
struct    AVCodecParserContext;
struct    AVCodecContext;
struct    AVFrame;
struct    AVPacket;


namespace H264
{
    using H264Frame = std::vector<uint8_t>;
    using H264Stream = std::vector<H264Frame>;

    using YuvFrame = std::vector<uint8_t>;
    using YuvStream = std::vector<YuvFrame>;

    class H264Decode:noncopyable
    {
        public:
            H264Decode();
            ~H264Decode();
            void SendH264Frame(const H264Frame& f);
            void ReceiveYuvFrame(YuvFrame*f);
            YuvStream ReceiveAllYuvFrame();
            void FlushDecode();
            bool Valid(){return valid;};
        private:
            void Reset();
            void Decode(AVPacket *p);
            bool InitMember();

        private:
            AVCodec* codec = nullptr;
            AVCodecParserContext* parser =nullptr;
            AVCodecContext*c = nullptr;
            AVFrame *frame = nullptr;
            AVPacket* pkt  = nullptr;
            std::deque<YuvFrame> stream_;
            bool valid = true;
    };
}
#endif
