#ifndef RTCP_XR_RRT_HPP
#define RTCP_XR_RRT_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include "rtprtcp_pub.hpp"

namespace cpp_streamer
{
/*
  https://datatracker.ietf.org/doc/html/rfc3611#page-21

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|reserved |   PT=XR=207   |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :                         report blocks                         :
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   rrt report block
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=4      |   reserved    |       block length = 2        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |              NTP timestamp, most significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |             NTP timestamp, least significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

typedef struct {
    uint8_t  bt;
    uint8_t  reserver;
    uint16_t block_length;
    uint32_t ntp_sec;
    uint32_t ntp_frac;
} XrRrtData;

inline void InitRrtBlock(XrRrtData* dlrr_block) {
    dlrr_block->bt = XR_RRT;
    dlrr_block->reserver = 0;
    dlrr_block->block_length = htons(2);
}

class XrRrt
{
public:
    XrRrt()
    {
        memset(data, 0, sizeof(data));
        data_len = sizeof(RtcpCommonHeader) + 4 + sizeof(XrRrtData);

        header = (RtcpCommonHeader*)data;
        header->version = 2;
        header->padding = 0;
        header->count   = 0;
        header->packet_type = RTCP_XR;
        header->length      = htons((sizeof(RtcpCommonHeader) + 4 + sizeof(XrRrtData))/4 - 1);

        ssrc_p = (uint32_t*)(header + 1);
        rrt_block = (XrRrtData*)(ssrc_p + 1);
        InitRrtBlock(rrt_block);
    }
    ~XrRrt()
    {
    }

public:
    void SetSsrc(uint32_t ssrc) {
        *ssrc_p = htonl(ssrc);
    }

    uint32_t GetSsrc() {
        return ntohl(*ssrc_p);
    }

    void SetNtp(uint32_t ntp_sec, uint32_t ntp_frac) {
        rrt_block->ntp_sec  = htonl(ntp_sec);
        rrt_block->ntp_frac = htonl(ntp_frac);
    }

    void GetNtp(uint32_t& ntp_sec, uint32_t& ntp_frac) {
        ntp_sec  = ntohl(rrt_block->ntp_sec);
        ntp_frac = ntohl(rrt_block->ntp_frac);
    }

    uint8_t* GetData() {
        return data;
    }

    size_t GetDataLen() {
        return data_len;
    }

    void parse(uint8_t* rtcp_data, size_t len) {
        assert(len == data_len);
        memcpy(data, rtcp_data, len);
    }

private:
    uint8_t data[RTP_PACKET_MAX_SIZE];
    size_t data_len = 0;
    RtcpCommonHeader* header = nullptr;
    uint32_t* ssrc_p           = nullptr;
    XrRrtData* rrt_block     = nullptr;
};

}
#endif
