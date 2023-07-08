#ifndef RTP_PACK_HPP
#define RTP_PACK_HPP

#include "net/rtprtcp/rtp_packet.hpp"
#include "format/h264_h265_header.hpp"


namespace cpp_streamer
{

inline RtpPacket* MakeRtpPacket(HeaderExtension* ext, size_t payload_len) {
    uint8_t* data = new uint8_t[RTP_PACKET_MAX_SIZE];
    size_t ext_len = 0;
    RtpCommonHeader* header = (RtpCommonHeader*)data;

    memset(header, 0, sizeof(RtpCommonHeader));
    header->version = RTP_VERSION;

    if (ext) {
        header->extension = 1;
        uint8_t* p = (uint8_t*)(header + 1);
        memcpy(p, ext, 4 + 4 * ntohs(ext->length));
        ext_len = 4 + 4 * ntohs(ext->length);
    } else {
        header->extension = 0;
        ext_len = 0;
    }

    size_t data_len = payload_len + sizeof(RtpCommonHeader) + ext_len;
    if (data_len > RTP_PACKET_MAX_SIZE) {
        return nullptr;
    }
    RtpPacket* packet = RtpPacket::Parse(data, data_len);
    packet->SetNeedDelete(true);

    return packet;
}

inline RtpPacket* GenerateSinglePackets(uint8_t* data, size_t len, HeaderExtension* ext = nullptr) {
    RtpPacket* packet = MakeRtpPacket(ext, len);

    uint8_t* payload = packet->GetPayload();

    memcpy(payload, data, len);
    packet->SetPayloadLength(len);
    return packet;

}
}

#endif


