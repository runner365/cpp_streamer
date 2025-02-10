#ifndef RTP_PACKET_HPP
#define RTP_PACKET_HPP
#include "rtprtcp_pub.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <arpa/inet.h>
#include <map>

namespace cpp_streamer
{
class Logger;

#define RTP_SEQ_MOD (1<<16)

typedef struct HeaderExtensionS
{
    uint16_t id;
    uint16_t length;
    uint8_t  value[1];
} HeaderExtension;

typedef struct OnebyteExtensionS {
    uint8_t len : 4;
    uint8_t id  : 4;
    uint8_t value[1];
} OnebyteExtension;

typedef struct TwobytesExtensionS {
    uint8_t id  : 8;
    uint8_t len : 8;
    uint8_t value[1];
} TwobytesExtension;

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

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      defined by profile       |           length              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        header extension                       |
   |                             ....                              |
 */

class RtpPacket
{
public:
    RtpPacket(RtpCommonHeader* header, HeaderExtension* ext,
            uint8_t* payload, size_t payload_len,
            uint8_t pad_len, size_t data_len);
    ~RtpPacket();

public:
    uint8_t Version() {return header_->version;}
    bool HasPadding() {return (header_->padding == 1) ? true : false;}
    void SetPadding(bool flag) {header_->padding = flag ? 1 : 0;}
    bool HasExtension() {return (header_->extension == 1) ? true : false;}
    uint8_t CsrcCount() {return header_->csrc_count;}
    uint8_t GetPayloadType() {return header_->payload_type;}
    void SetPayloadType(uint8_t type) {header_->payload_type = type;}
    uint8_t GetMPayloadType() {
        uint8_t marker = header_->marker;
        return (marker << 7) | header_->payload_type;
    }
    uint8_t GetMarker() {return header_->marker;}
    void SetMarker(uint8_t marker) { header_->marker = marker; }
    uint16_t GetSeq() {return ntohs(header_->sequence);}
    void SetSeq(uint16_t seq) {header_->sequence = htons(seq);}
    uint32_t GetTimestamp() {return ntohl(header_->timestamp);}
    void SetTimestamp(uint32_t ts) { header_->timestamp = (uint32_t)htonl(ts); }
    uint32_t GetSsrc() {return ntohl(header_->ssrc);}
    void SetSsrc(uint32_t ssrc) {header_->ssrc = (uint32_t)htonl(ssrc);}

    uint8_t* GetData() {return (uint8_t*)header_;}
    size_t GetDataLength() {return data_len_;}

    uint8_t* GetPayload() {return payload_;}
    size_t GetPayloadLength() {return payload_len_;}
    void SetPayloadLength(size_t len) { payload_len_ = len; }

    void SetMidExtensionId(uint8_t id) { mid_extension_id_ = id; }
    uint8_t GetMidExtensionId() { return mid_extension_id_; }

    void SetAbsTimeExtensionId(uint8_t id) { abs_time_extension_id_ = id; }
    uint8_t GetAbsTimeExtensionId() { return abs_time_extension_id_; }

    void SetTransportWideCcExtensionId(uint8_t id) { transport_wideCc_extension_id_ = id; }
    uint8_t GetTransportWideCcExtensionId() { return transport_wideCc_extension_id_; }

    bool UpdateMid(uint8_t mid);
    bool ReadMid(uint8_t& mid);

    bool ReadAbsTime(uint32_t& abs_time_24bits);
    bool UpdateAbsTime(uint32_t abs_time_24bits);

    bool GetTransportWideSeq(uint16_t& seq);
    bool UpdateTransportWideSeq(uint16_t seq);

    void SetNeedDelete(bool flag) { need_delete_ = flag; }
    bool GetNeedDelete() { return need_delete_; }
    void EnableDebug() { debug_enable_ = true; }
    void DisableDebug() { debug_enable_ = false; }
    bool IsDebug() { return debug_enable_; }
    
    int64_t GetLocalMs() {return local_ms_;}

    void RtxDemux(uint32_t ssrc, uint8_t payloadtype);
    void RtxMux(uint8_t payload_type, uint32_t ssrc, uint16_t seq);

    std::string Dump();
    void SetLogger(Logger* logger) { logger_ = logger; }

public:
    static RtpPacket* Parse(uint8_t* data, size_t len);
    RtpPacket* Clone(uint8_t* buffer = nullptr);

private:
    void ParseExt();
    void ParseOnebyteExt();
    void ParseTwobytesExt();
    uint16_t GetExtId(HeaderExtension* rtp_ext);
    uint16_t GetExtLength(HeaderExtension* rtp_ext);
    uint8_t* GetExtValue(HeaderExtension* rtp_ext);
    bool HasOnebyteExt(HeaderExtension* rtp_ext);
    bool HasTwobytesExt(HeaderExtension* rtp_ext);

    uint8_t* GetExtension(uint8_t id, uint8_t& len);

    bool UpdateExtensionLength(uint8_t id, uint8_t len);

private:
    RtpCommonHeader* header_    = nullptr;
    HeaderExtension* ext_       = nullptr;
    uint8_t* payload_           = nullptr;
    size_t payload_len_         = 0;
    uint8_t pad_len_            = 0;
    size_t data_len_            = 0;
    int64_t local_ms_           = 0;
    bool need_delete_           = false;
    bool debug_enable_          = false;

private:
    uint8_t mid_extension_id_               = 0;
    uint8_t abs_time_extension_id_          = 0;
    uint8_t transport_wideCc_extension_id_  = 0;

private:
    std::map<uint8_t, OnebyteExtension*>  onebyte_ext_map_;
    std::map<uint8_t, TwobytesExtension*> twobytes_ext_map_;

private:
    Logger* logger_ = nullptr;
};

}

#endif

