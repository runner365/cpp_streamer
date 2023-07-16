#ifndef RTCP_PS_PLI_HPP
#define RTCP_PS_PLI_HPP
#include "rtprtcp_pub.hpp"
#include "rtcp_fb_pub.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "stringex.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <sstream>

namespace cpp_streamer
{

class RtcpPsPli
{
public:
    RtcpPsPli() {
        memset(this->data, 0, sizeof(this->data));
        header_ = (RtcpFbCommonHeader*)(this->data);
        fb_header_ = (RtcpFbHeader*)(header_ + 1);

        //init field...
        header_->version     = 2;
        header_->padding     = 0;
        header_->fmt         = (uint8_t)FB_PS_PLI;
        header_->packet_type = (uint8_t)RTCP_PSFB;
        header_->length      = htons(2);
    }

    ~RtcpPsPli() {
    }

    static RtcpPsPli* Parse(uint8_t* data, size_t len) {
        if (len != (sizeof(RtcpFbCommonHeader) + sizeof(RtcpFbHeader))) {
            return nullptr;
        }
        RtcpPsPli* pkt = new RtcpPsPli();
        memcpy(pkt->data, data, len);
        pkt->header_ = (RtcpFbCommonHeader*)(pkt->data);
        pkt->fb_header_ = (RtcpFbHeader*)(pkt->header_ + 1);

        return pkt;
    }

public:
    void SetSenderSsrc(uint32_t sender_ssrc) { fb_header_->sender_ssrc = (uint32_t)htonl(sender_ssrc);}
    void SetMediaSsrc(uint32_t media_ssrc) { fb_header_->media_ssrc = (uint32_t)htonl(media_ssrc);}
    uint32_t GetSenderSsrc() { return (uint32_t)ntohl(fb_header_->sender_ssrc); }
    uint32_t GetMediaSsrc() { return (uint32_t)ntohl(fb_header_->media_ssrc); }

    uint8_t* GetData() { return this->data; }
    size_t GetDataLen() { return sizeof(RtcpFbCommonHeader) + sizeof(RtcpFbHeader); }

    std::string Dump() {
        std::stringstream ss;
        
        ss << "rtcp ps feedback length:" << this->GetDataLen();
        ss << ", sender ssrc:" << this->GetSenderSsrc();
        ss << ", media ssrc:" << this->GetMediaSsrc() << "\r\n";
        return ss.str();
    }

private:
    uint8_t data[RTP_PACKET_MAX_SIZE];
    RtcpFbCommonHeader* header_ = nullptr;
    RtcpFbHeader* fb_header_    = nullptr;
};

}

#endif
