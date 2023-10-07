#ifndef RTCP_FEEDBACK_NACK_HPP
#define RTCP_FEEDBACK_NACK_HPP
#include "rtcp_fb_pub.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
#include <sstream>
#include <stdio.h>
#include <arpa/inet.h>  // htonl(), htons(), ntohl(), ntohs()
#include <vector>

namespace cpp_streamer
{
/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|  FMT=1  |   PT=205      |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                          sender ssrc                          |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                           media ssrc                          |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |     packet identifier         |     bitmap of loss packets    |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

typedef struct
{
    uint16_t packet_id;//base seq
    uint16_t lost_bitmap;
} RtcpNackBlock;

class RtcpFbNack
{
public:
    RtcpFbNack(uint32_t sender_ssrc, uint32_t media_ssrc)
    {
        fb_common_header_ = (RtcpFbCommonHeader*)(this->data);
        nack_header_ = (RtcpFbHeader*)(fb_common_header_ + 1);
        this->data_len = sizeof(RtcpFbCommonHeader) + sizeof(RtcpFbHeader);

        fb_common_header_->version     = 2;
        fb_common_header_->padding     = 0;
        fb_common_header_->fmt         = (int)FB_RTP_NACK;
        fb_common_header_->packet_type = RTCP_RTPFB;
        fb_common_header_->length      = (uint16_t)htons((uint16_t)this->data_len/4 - 1);

        nack_header_->sender_ssrc = (uint32_t)htonl(sender_ssrc);
        nack_header_->media_ssrc  = (uint32_t)htonl(media_ssrc);
    }

    ~RtcpFbNack()
    {

    }

public:
    static RtcpFbNack* Parse(uint8_t* data, size_t len) {
        if (len <= sizeof(RtcpFbCommonHeader) + sizeof(RtcpFbHeader)) {
            return nullptr;
        }
        
        RtcpFbNack* pkt = new RtcpFbNack(0, 0);
        (void)pkt->UpdateData(data, len);
        return pkt;
    }


    void InsertSeqList(const std::vector<uint16_t>& seq_vec) {
        std::vector<uint16_t> report_seqs;

        for (size_t index = 0; index < seq_vec.size(); index++) {
            uint16_t lost_seq = seq_vec[index];
            report_seqs.push_back(lost_seq);

            if ((report_seqs[report_seqs.size() - 1] - report_seqs[0]) > 16) {
                InsertBlock(report_seqs);
                report_seqs.clear();
            }
        }
        if (!report_seqs.empty()) {
            InsertBlock(report_seqs);
            report_seqs.clear();
        }
    }

    void InsertBlock(const std::vector<uint16_t>& report_seqs) {
        uint16_t packet_id = report_seqs[0];
        uint16_t bitmap    = 0;

        for (size_t r = 1; r < report_seqs.size(); r++) {
            uint16_t temp_seq = report_seqs[r];
            bitmap |= 1 << (16 - (temp_seq - packet_id));
        }
        RtcpNackBlock* block = (RtcpNackBlock*)(this->data + this->data_len);
        block->packet_id   = htons(packet_id);
        block->lost_bitmap = htons(bitmap);

        nack_blocks_.push_back(block);

        this->data_len += sizeof(RtcpNackBlock);
        fb_common_header_->length = htons((uint16_t)(this->data_len/4 - 1));
    }

    std::vector<uint16_t> GetLostSeqs() {
        std::vector<uint16_t> seqs;

        for (auto block : nack_blocks_) {
            uint16_t seq = ntohs(block->packet_id);
            
            seqs.push_back(seq);
            seq++;
            for (uint16_t bit_mask = ntohs(block->lost_bitmap);
                bit_mask != 0;
                bit_mask >>= 1, ++seq) {
                
                if (bit_mask & 0x01) {
                    seqs.push_back(seq);
                }
            }
        }

        return seqs;
    }

public:
    uint32_t GetSenderSsrc() {return (uint32_t)ntohl(nack_header_->sender_ssrc);}
    uint32_t GetMediaSsrc() {return (uint32_t)ntohl(nack_header_->media_ssrc);}
    uint8_t* GetData() {return this->data;}
    size_t GetLen() {return this->data_len;}
    size_t GetPayloadLen() {
        RtcpFbCommonHeader* header = (RtcpFbCommonHeader*)(this->data);
        return (size_t)ntohs(header->length) * 4;
    }
    uint16_t GetBaseSeq(RtcpNackBlock* block) {
        return ntohs(block->packet_id);
    }
    uint16_t GetBitask(RtcpNackBlock* block) {
        return ntohs(block->lost_bitmap);
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "rtcp fb nack: sender ssrc=" << GetSenderSsrc() << ", media ssrc=" << GetMediaSsrc() << "\r\n";
        
        for (auto block : nack_blocks_) {
            char bitmap_sz[80];
            snprintf(bitmap_sz, sizeof(bitmap_sz), "0x%04x", ntohs(block->lost_bitmap));
            ss << " base seq:" << ntohs(block->packet_id) << ", " << "bitmap:" << std::string(bitmap_sz) << "\r\n";
        }
        char print_data[4*1024];
        size_t print_len = 0;
        const size_t max_print = 256;
        size_t len = this->data_len;
    
        for (size_t index = 0; index < (len > max_print ? max_print : len); index++) {
            if ((index%16) == 0) {
                print_len += snprintf(print_data + print_len, sizeof(print_data) - print_len, "\r\n");
            }
            
            print_len += snprintf(print_data + print_len, sizeof(print_data) - print_len,
                " %02x", this->data[index]);
        }
        ss << std::string(print_data) << "\r\n";

        return ss.str();
    }
private:
    bool UpdateData(uint8_t* data, size_t len) {
        if (len <= sizeof(RtcpFbCommonHeader) + sizeof(RtcpFbHeader)) {
            return false;
        }
        memcpy(this->data, data, len);
        this->data_len = len;
        fb_common_header_ = (RtcpFbCommonHeader*)(this->data);
        RtcpFbHeader* nack_header = (RtcpFbHeader*)(fb_common_header_ + 1);
        RtcpNackBlock* block = (RtcpNackBlock*)(nack_header + 1);
        uint8_t* p = (uint8_t*)block;

        while ((size_t)(p - this->data) < this->data_len) {
            nack_blocks_.push_back(block);
            block++;
            p = (uint8_t*)block;
        }
        return true;
    }

public:
    uint8_t data[1500];
    size_t  data_len = 0;
    RtcpFbCommonHeader* fb_common_header_ = nullptr;
    RtcpFbHeader* nack_header_        = nullptr;
    std::vector<RtcpNackBlock*>  nack_blocks_;
};

}
#endif
