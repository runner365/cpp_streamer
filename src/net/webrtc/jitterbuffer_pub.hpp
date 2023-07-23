#ifndef JITTER_BUFFER_PUB_HPP
#define JITTER_BUFFER_PUB_HPP

#include "rtp_packet.hpp"
#include "av.hpp"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <memory>

namespace cpp_streamer
{
#define JITTER_BUFFER_AUDIO_TIMEOUT 100 //ms
#define JITTER_BUFFER_VIDEO_TIMEOUT 400 //ms

class RtpPacketInfo
{
public:
    RtpPacketInfo(MEDIA_PKT_TYPE media_type,
            int clock_rate, 
            RtpPacket* input_pkt, 
            int64_t extend_seq):media_type_(media_type)
                                , extend_seq_(extend_seq)
                                , clock_rate_(clock_rate)
    {
        this->pkt = input_pkt->Clone();
    }

    ~RtpPacketInfo()
    {
        delete this->pkt;
        this->pkt = nullptr;
    }

public:
    MEDIA_PKT_TYPE media_type_;
    RtpPacket* pkt = nullptr;
    int64_t extend_seq_ = 0;
    int clock_rate_     = 0;
};

class JitterBufferCallbackI
{
public:
    virtual void RtpPacketReset(std::shared_ptr<RtpPacketInfo> pkt_ptr) = 0;
    virtual void RtpPacketOutput(std::shared_ptr<RtpPacketInfo> pkt_ptr) = 0;
};

}
#endif
