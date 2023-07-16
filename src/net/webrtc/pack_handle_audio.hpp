#ifndef PACK_HANDLE_AUDIO_HPP
#define PACK_HANDLE_AUDIO_HPP
#include "pack_handle_pub.hpp"
#include "utils/av/media_packet.hpp"
#include "logger.hpp"
#include <memory>
#include <stdint.h>
#include <stddef.h>
#include <string>

namespace cpp_streamer
{

class PackHandleAudio : public PackHandleBase
{
public:
    PackHandleAudio(PackCallbackI* cb, Logger* logger):cb_(cb)
                                                       , logger_(logger)
    {
    }
    virtual ~PackHandleAudio()
    {
    }

public:
    virtual void InputRtpPacket(std::shared_ptr<RtpPacketInfo> pkt_ptr) override
    {
        size_t pkt_size = pkt_ptr->pkt->GetPayloadLength() + 1024;
        std::shared_ptr<Media_Packet> audio_pkt_ptr = std::make_shared<Media_Packet>(pkt_size);
        int64_t dts = (int64_t)pkt_ptr->pkt->GetTimestamp();

        audio_pkt_ptr->av_type_    = MEDIA_AUDIO_TYPE;
        audio_pkt_ptr->codec_type_ = MEDIA_CODEC_OPUS;
        audio_pkt_ptr->fmt_type_   = MEDIA_FORMAT_RAW;
        audio_pkt_ptr->dts_        = dts;
        audio_pkt_ptr->pts_        = dts;

        LogDebugf(logger_, "audio packet dts:%ld, payload:%lu", dts, pkt_ptr->pkt->GetPayloadLength());
        audio_pkt_ptr->buffer_ptr_->AppendData((char*)pkt_ptr->pkt->GetPayload(), pkt_ptr->pkt->GetPayloadLength());
        cb_->MediaPacketOutput(audio_pkt_ptr);
    }

private:
    PackCallbackI* cb_ = nullptr;
    Logger* logger_    = nullptr;
};

}

#endif
