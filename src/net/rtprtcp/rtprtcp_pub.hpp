#ifndef RTP_RTCP_PUB_HPP
#define RTP_RTCP_PUB_HPP

#include <string>
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <sstream>
#include <arpa/inet.h>  // htonl(), htons(), ntohl(), ntohs()

namespace cpp_streamer
{

#define RTP_PACKET_MAX_SIZE 1500
#define RTP_PAYLOAD_MAX_SIZE 1200

#define SEQUENCE_MAX 65535

#define RTP_VERSION         2 // RTP Version must be 2.

typedef enum
{
    RTCP_SR    = 200,
    RTCP_RR    = 201,
    RTCP_SDES  = 202,
    RTCP_BYE   = 203,
    RTCP_APP   = 204,
    RTCP_RTPFB = 205,
    RTCP_PSFB  = 206,
    RTCP_XR    = 207
} RTCP_TYPE;

typedef enum
{
    FB_RTP_NACK   = 1,
    FB_RTP_TMMBR  = 3,
    FB_RTP_TMMBN  = 4,
    FB_RTP_SR_REQ = 5,
    FB_RTP_RAMS   = 6,
    FB_RTP_TLLEI  = 7,
    FB_RTP_ECN    = 8,
    FB_RTP_PS     = 9,
    FB_RTP_TCC    = 15,
    FB_RTP_EXT    = 31
} RTCP_FB_RTP_FMT;

typedef enum
{
    FB_PS_PLI   = 1,
    FB_PS_SLI   = 2,
    FB_PS_RPSI  = 3,
    FB_PS_FIR   = 4,
    FB_PS_TSTR  = 5,
    FB_PS_TSTN  = 6,
    FB_PS_VBCM  = 7,
    FB_PS_PSLEI = 8,
    FB_PS_ROI   = 9,
    FB_PS_AFB   = 15,
    FB_PS_EXT   = 31
} RTCP_FB_PS_FMT;

typedef enum
{
    XR_LRLE = 1,
    XR_DRLE = 2,
    XR_PRT  = 3,
    XR_RRT  = 4,
    XR_DLRR = 5,
    XR_SS   = 6,
    XR_VM   = 7
} XR_BLOCK_TYPE;

/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    RC   |   PT=SR=200   |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct RtcpCommonHeaderS
{
    uint8_t count : 5;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t packet_type : 8;
    uint16_t length;
} RtcpCommonHeader;

/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|  FMT    |       PT      |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct RtcpFbCommonHeaderS
{
    uint8_t fmt : 5;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t packet_type : 8;
    uint16_t length : 16;
} RtcpFbCommonHeader;

typedef struct RtcpXrHeaderS
{
    uint8_t  bt;
    uint8_t  reserver;
    uint16_t block_length;
} RtcpXrHeader;

/**
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct RtpCommonHeaderS
{
    uint8_t csrc_count : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t payload_type : 7;
    uint8_t marker : 1;
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
} RtpCommonHeader;

inline std::string RtpHeaderDump(RtpCommonHeader* header) {
    std::stringstream ss;

    ss << "rtp common header, version:" << header->version
        << ", padding:" << header->padding
        << ", extension:" << header->extension
        << ", csrc_count:" << header->csrc_count
        << ", marker:" << header->marker
        << ", seq:" << header->sequence
        << ", ts:" << header->timestamp
        << ", ssrc:" << header->ssrc;
    return ss.str();
}

inline bool IsRtcp(const uint8_t* data, size_t len) {
    RtcpCommonHeader* header = (RtcpCommonHeader*)data;

    return ((len >= sizeof(RtcpCommonHeader)) &&
        // DOC: https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
        (data[0] > 127 && data[0] < 192) &&
        (header->version == 2) &&
        // http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-4
        (header->packet_type >= 192 && header->packet_type <= 223));
}

inline bool IsRtp(const uint8_t* data, size_t len) {
    // NOTE: is_rtcp must be called before this method.
    RtpCommonHeader* header = (RtpCommonHeader*)data;

    return ((len >= sizeof(RtpCommonHeader)) &&
        // DOC: https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
        (data[0] > 127 && data[0] < 192) && (header->version == 2));
}

inline uint16_t GetRtcpLength(RtcpCommonHeader* header) {
    return ntohs(header->length) * 4;
}

inline std::string RtcpHeaderDump(RtcpCommonHeader* header) {
    std::stringstream ss;

    ss << "rtcp common header, version:" << (int)header->version << ", padding:" << (int)header->padding << ", count:" << (int)header->count;
    ss << ", payloadtype:" << (int)header->packet_type << ", length:" << GetRtcpLength(header) << "\r\n";

    return ss.str();
}

inline bool SeqLowerThan(uint16_t seq1, uint16_t seq2) {
    if ((seq1 > seq2) && ((seq1 - seq2) > SEQUENCE_MAX/2)) {
        return true;
    }

    if ((seq1 < seq2) && ((seq2 - seq1) <= SEQUENCE_MAX/2)) {
        return true;
    }
    return false;
}

inline bool SeqHigherThan(uint16_t seq1, uint16_t seq2) {
    if (seq1 == seq2) {
        return false;
    }
    bool ret = SeqLowerThan(seq1, seq2);
    return !ret;
}

}

#endif //RTP_RTCP_PUB_HPP
