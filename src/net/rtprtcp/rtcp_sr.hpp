#ifndef RTCP_SR_HPP
#define RTCP_SR_HPP
#include "logger.hpp"
#include "rtprtcp_pub.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <sstream>

namespace cpp_streamer
{
/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    RC   |   PT=SR=200   |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         SSRC of sender                        |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
sender |              NTP timestamp, most significant word             |
info   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |             NTP timestamp, least significant word             |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         RTP timestamp                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                     sender's packet count                     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      sender's octet count                     |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/

typedef struct {
    uint32_t ssrc;
    uint32_t ntp_sec;
    uint32_t ntp_frac;
    uint32_t rtp_timestamp;
    uint32_t pkt_count;
    uint32_t bytes_count;
} RtcpSrHeader;

class RtcpSrPacket
{
public:
    RtcpSrPacket(RtcpCommonHeader* rtcp_header) {
        memcpy(this->data, (uint8_t*)rtcp_header, sizeof(RtcpCommonHeader) + sizeof(RtcpSrHeader));
        this->rtcp_header_ = (RtcpCommonHeader*)(this->data);
        this->header_      = (RtcpSrHeader*)(this->data + sizeof(RtcpCommonHeader));
    }

    RtcpSrPacket() {
        memset(this->data, 0, sizeof(this->data));
        rtcp_header_ = (RtcpCommonHeader*)(this->data);
        rtcp_header_->version     = 2;
        rtcp_header_->padding     = 0;
        rtcp_header_->count       = 0;
        rtcp_header_->packet_type = (uint8_t)RTCP_SR;
        rtcp_header_->length      = (uint32_t)htons(sizeof(RtcpSrHeader)/sizeof(uint32_t));
        this->header_             = (RtcpSrHeader*)(this->data + sizeof(RtcpCommonHeader));
    }

    ~RtcpSrPacket() {

    }

    void SetSsrc(uint32_t ssrc) {
        header_->ssrc = (uint32_t)htonl(ssrc);
    }

    uint32_t GetSsrc() {
        return ntohl(header_->ssrc);
    }

    void SetNtp(uint32_t ntp_sec, uint32_t ntp_frac) {
        header_->ntp_sec  = (uint32_t)htonl(ntp_sec);
        header_->ntp_frac = (uint32_t)htonl(ntp_frac);
    }

    uint32_t GetNtpSec() {
        return ntohl(header_->ntp_sec);
    }

    uint32_t GetNtpFrac() {
        return ntohl(header_->ntp_frac);
    }

    void SetRtpTimestamp(uint32_t ts) {
        header_->rtp_timestamp = (uint32_t)htonl(ts);
    }

    uint32_t GetRtpTimestamp() {
        return ntohl(header_->rtp_timestamp);
    }

    void SetPktCount(uint32_t pkt_count) {
        header_->pkt_count = (uint32_t)htonl(pkt_count);
    }

    uint32_t GetPktCount() {
        return ntohl(header_->pkt_count);
    }

    void SetBytesCount(uint32_t bytes_count) {
        header_->bytes_count = (uint32_t)htonl(bytes_count);
    }

    uint32_t GetBytesCount() {
        return ntohl(header_->bytes_count);
    }

public:
    static RtcpSrPacket* Parse(uint8_t* data, size_t len) {
        if (len != (sizeof(RtcpCommonHeader) + sizeof(RtcpSrHeader))) {
            CSM_THROW_ERROR("rtcp sr len(%lu) error", len);
        }
        RtcpCommonHeader* rtcp_header = (RtcpCommonHeader*)data;

        return new RtcpSrPacket(rtcp_header);
    }

    uint8_t* Serial(size_t& ret_len) {
        uint8_t* ret_data = this->data;
        ret_len = sizeof(RtcpCommonHeader) + sizeof(RtcpSrHeader);

        return ret_data;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "rtcp version:" << (int)rtcp_header_->version << ", pad:" << (int)rtcp_header_->padding
           << ", rc:" << (int)rtcp_header_->count << ", rtcp type:" << (int)rtcp_header_->packet_type
           << ", length:" << (int)rtcp_header_->length << "\r\n";
        ss << "  ssrc:" << header_->ssrc << ", ntp sec:" << header_->ntp_sec << ", ntp frac:"
           << header_->ntp_frac << ", rtp timestamp:" << header_->rtp_timestamp
           << ", packet count:" << header_->pkt_count << ", bytes:"
           << header_->bytes_count << "\r\n";

        return ss.str();
    }

    uint8_t* GetData() { return this->data; }
    size_t GetDataLen() { return sizeof(RtcpCommonHeader) + sizeof(RtcpSrHeader); }

private:
    RtcpCommonHeader* rtcp_header_ = nullptr;
    RtcpSrHeader* header_          = nullptr;
    uint8_t data[1500];
};

}

#endif