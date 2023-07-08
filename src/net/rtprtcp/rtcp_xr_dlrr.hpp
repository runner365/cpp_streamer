#ifndef RTCP_XR_DLRR_HPP
#define RTCP_XR_DLRR_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <string.h>
#include <arpa/inet.h>
#include <vector>
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

 dlrr block
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |     BT=5      |   reserved    |         block length          |
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 |                 SSRC_1 (SSRC of first receiver)               | sub-
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
 |                         last RR (LRR)                         |   1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                   delay since last RR (DLRR)                  |
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 |                 SSRC_2 (SSRC of second receiver)              | sub-
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
 :                               ...                             :   2
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/

typedef struct {
    uint8_t  bt;
    uint8_t  reserver;
    uint16_t block_length;
    uint32_t ssrc;
    uint32_t lrr;
    uint32_t dlrr;
} XrDlrrData;

inline void InitDlrrBlock(XrDlrrData* dlrr_block) {
    dlrr_block->bt = XR_DLRR;
    dlrr_block->reserver = 0;
    dlrr_block->block_length = htons(3);
}

class XrDlrr
{
public:
    XrDlrr()
    {
        memset(data, 0, sizeof(data));
        header = (RtcpCommonHeader*)data;
        header->version = 2;
        header->padding = 0;
        header->count   = 0;
        header->packet_type = RTCP_XR;
        header->length      = 0;

        ssrc_p = (uint32_t*)(header + 1);
    }
    ~XrDlrr()
    {
    }

public:
    void SetSsrc(uint32_t ssrc) {
        *ssrc_p = htonl(ssrc);
    }

    uint32_t GetSsrc() {
        return ntohl(*ssrc_p);
    }

    void AddrDlrrBlock(uint32_t ssrc, uint32_t lrr, uint32_t dlrr) {
        if (dlrr_block == nullptr) {
            ssrc_p = (uint32_t*)(header + 1);
            dlrr_block = (XrDlrrData*)(ssrc_p + 1);
        } else {
            dlrr_block = dlrr_block + 1;
        }
        InitDlrrBlock(dlrr_block);

        dlrr_block->ssrc = htonl(ssrc);
        dlrr_block->lrr  = htonl(lrr);
        dlrr_block->dlrr = htonl(dlrr);

        block_count++;
        
        header->length = htons((uint16_t)(4 + sizeof(XrDlrrData) * block_count) / 4);
        data_len = sizeof(RtcpCommonHeader) + 4 + sizeof(XrDlrrData) * block_count;
        return;
    }

    std::vector<XrDlrrData> GetDlrrBlocks() {
        std::vector<XrDlrrData> blocks;
        XrDlrrData* dlrr_block_item = (XrDlrrData*)(ssrc_p + 1);
        size_t count = 0;

        if (data_len < (sizeof(RtcpCommonHeader) + 4 + sizeof(XrDlrrData))) {
            return blocks;
        }

        count = (data_len - sizeof(RtcpCommonHeader) - 4) / sizeof(XrDlrrData);
        for (size_t index = 0; index < count; index++) {
            XrDlrrData item;

            item.bt           = dlrr_block_item->bt;
            item.reserver     = dlrr_block_item->reserver;
            item.block_length = ntohs(dlrr_block_item->block_length);
            item.ssrc         = ntohl(dlrr_block_item->ssrc);
            item.lrr          = ntohl(dlrr_block_item->lrr);
            item.dlrr         = ntohl(dlrr_block_item->dlrr);

            blocks.push_back(item);
            dlrr_block_item++;
        }
        return blocks;
    }
    
    uint8_t* GetData() {
        return data;
    }

    size_t GetDataLen() {
        return data_len;
    }

private:
    uint8_t data[RTP_PACKET_MAX_SIZE];
    size_t data_len = 0;
    RtcpCommonHeader* header = nullptr;
    uint32_t* ssrc_p           = nullptr;
    XrDlrrData* dlrr_block   = nullptr;
    size_t block_count         = 0;
};

}
#endif

