#include "rtp_packet.hpp"
#include "rtprtcp_pub.hpp"
#include "logger.hpp"
#include "timeex.hpp"
#include "byte_stream.hpp"
#include <arpa/inet.h>
#include <sstream>
#include <cstring>
#include <assert.h>

namespace cpp_streamer
{

RtpPacket* RtpPacket::Parse(uint8_t* data, size_t len) {
    RtpCommonHeader* header = (RtpCommonHeader*)data;
    HeaderExtension* ext = nullptr;
    uint8_t* p = (uint8_t*)(header + 1);

    if (len > RTP_PACKET_MAX_SIZE) {
        CSM_THROW_ERROR("rtp len(%lu) is to large", len);
    }

    if (header->csrc_count > 0) {
        p += 4 * header->csrc_count;
    }

    if (header->extension) {
        if (len < (size_t)(p - data + 4)) {
            CSM_THROW_ERROR("rtp len(%lu) is to small", len);
        }
        ext = (HeaderExtension*)p;
        size_t extension_byte = (size_t)(ntohs(ext->length) * 4);
        if (len < (size_t)(p - data + 4 + extension_byte)) {
            CSM_THROW_ERROR("rtp len(%lu) is to small", len);
        }
        p += 4 + extension_byte;//4bytes(externsion header) + extersion bytes
    }

    if (len <= (size_t)(p - data)) {
        CSM_THROW_ERROR("rtp len(%lu) is to small, has no payload", len);
    }
    uint8_t* payload = p;
    size_t payload_len = len - (size_t)(p - data);
    uint8_t pad_len = 0;

    if (header->padding) {
        pad_len = data[len - 1];
        if (pad_len > 0) {
            if (payload_len <= pad_len) {
                CSM_THROW_ERROR("rtp payload length(%lu), pad length(%d) error",
                        payload_len, pad_len);
            }
            payload_len -= pad_len;
        }
    }

    RtpPacket* pkt = new RtpPacket(header, ext, payload, payload_len, pad_len, len);

    return pkt;
}

RtpPacket::RtpPacket(RtpCommonHeader* header, HeaderExtension* ext,
                uint8_t* payload, size_t payload_len,
                uint8_t pad_len, size_t data_len) {
    if (data_len > RTP_PACKET_MAX_SIZE) {
        CSM_THROW_ERROR("rtp len(%lu) is to large", data_len);
    }
    header_      = header;
    ext_         = ext;
    payload_     = payload;
    payload_len_ = payload_len;
    pad_len_     = pad_len;

    data_len_    = data_len;

    local_ms_    = (int64_t)now_millisec();

    ParseExt();
    need_delete_ = false;
}

RtpPacket::~RtpPacket() {
    uint8_t* data = (uint8_t*)header_;
    if (need_delete_ && data) {
        delete[] data;
    }
}

RtpPacket* RtpPacket::Clone(uint8_t* buffer) {
    uint8_t* new_data = nullptr;
    
    if (buffer) {
        new_data = buffer;
    } else {
        new_data = new uint8_t[RTP_PACKET_MAX_SIZE];
    }
    
    assert(GetDataLength() < RTP_PACKET_MAX_SIZE);
    memcpy(new_data, GetData(), GetDataLength());

    RtpPacket* new_pkt = RtpPacket::Parse(new_data, GetDataLength());

    if (buffer) {
        new_pkt->need_delete_ = false;
    } else {
        new_pkt->need_delete_ = true;
    }
    
    new_pkt->mid_extension_id_ = mid_extension_id_;
    new_pkt->abs_time_extension_id_ = abs_time_extension_id_;

    return new_pkt;
}

std::string RtpPacket::Dump() {
    std::stringstream ss;
    char desc[128];

    snprintf(desc, sizeof(desc), "%p", GetData());

    ss << "rtp packet data:" << desc << ", data length:" << data_len_ << "\r\n";
    ss << "  version:" << (int)Version() << ", padding:" << HasPadding();
    ss << ", extension:" << HasExtension() << ", csrc count:" << (int)CsrcCount() << "\r\n";
    ss << "  marker:" << (int)GetMarker() << ", payload type:" << (int)GetPayloadType() << "\r\n";
    ss << "  sequence:" << (int)GetSeq() << ", timestamp:" << GetTimestamp();
    ss << ", ssrc:" << GetSsrc() << "\r\n";

    snprintf(desc, sizeof(desc), "%p", GetPayload());
    ss << "  payload:" << desc << ", payload length:" << GetPayloadLength() << "\r\n";

    if (HasPadding()) {
        uint8_t* media_data = GetData();
        ss << "  padding len:" << media_data[data_len_ - 1] << "\r\n";
    }

    if (HasExtension()) {
        if (!onebyte_ext_map_.empty()) {
            ss <<  "  rtp onebyte extension:" << "\r\n";
            for (auto item : onebyte_ext_map_) {
                OnebyteExtension* item_ext = item.second;
                ss << "    id:" << (int)item.first << ", length:" << (int)(item_ext->len) << "\r\n";
                if (item.first == mid_extension_id_) {
                    std::string mid_str((char*)(item_ext->value), (int)item_ext->len + 1);
                    ss << "      mid:" << mid_str << "\r\n";
                } else if (item.first == abs_time_extension_id_) {
                    uint32_t abs_time_24bits = ByteStream::Read3Bytes(item_ext->value);
                    double send_ms = abs_time_to_ms(abs_time_24bits);
                    ss << "      abs time:" << send_ms << "\r\n";
                }
            }
        }

        if (!twobytes_ext_map_.empty()) {
            ss << "  rtp twobytes extension:" << "\r\n";
            for ( auto item : twobytes_ext_map_) {
                TwobytesExtension* item_ext = item.second;
                ss << "    id:" << (int)item.first << ", length:" << (int)item_ext->len << "\r\n";
                if (item.first == mid_extension_id_) {
                    std::string mid_str((char*)(item_ext->value), (int)item_ext->len + 1);
                    ss << "      mid:" << mid_str << "\r\n";
                }
            }
        }
    }
    return ss.str();
}

uint16_t RtpPacket::GetExtId(HeaderExtension* rtp_ext) {
    if (rtp_ext == nullptr) {
        return 0;
    }
    
    return ntohs(rtp_ext->id);
}

uint16_t RtpPacket::GetExtLength(HeaderExtension* rtp_ext) {
    if (rtp_ext == nullptr) {
        return 0;
    }
    
    return ntohs(rtp_ext->length) * 4;
}

uint8_t* RtpPacket::GetExtValue(HeaderExtension* rtp_ext) {
    if (rtp_ext == nullptr) {
        return 0;
    }
    
    return rtp_ext->value;
}

void RtpPacket::ParseExt() {
    if ((header_->extension == 0) || (ext_ == nullptr)) {
        return;
    }

    //base on rfc5285
    if (HasOnebyteExt(ext_)) {
        ParseOnebyteExt();
    } else if (HasTwobytesExt(ext_)) {
        ParseTwobytesExt();
    } else {
        CSM_THROW_ERROR("the rtp extension id(%02x) error", ext_->id);
    }
}

void RtpPacket::ParseOnebyteExt() {
    onebyte_ext_map_.clear();

    uint8_t* ext_start = (uint8_t*)(ext_) + 4;//skip id(16bits) + length(16bits)
    uint8_t* ext_end   = ext_start + GetExtLength(ext_);
    uint8_t* p = ext_start;

/*
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |       0xBE    |    0xDE       |           length=3            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  ID   | L=0   |     data      |  ID   |  L=1  |   data...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            ...data   |    0 (pad)    |    0 (pad)    |  ID   | L=3   |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          data                                 |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
    while (p < ext_end) {
        uint8_t id = (*p & 0xF0) >> 4;
        size_t len = (size_t)(*p & 0x0F) + 1;

        if (id == 0x0f)
            break;

        if (id != 0) {
            if (p + 1 + len > ext_end) {
                CSM_THROW_ERROR("rtp extension length(%d) is not enough in one byte extension mode.",
                    GetExtLength(ext_));
                break;
            }

            onebyte_ext_map_[id] = (OnebyteExtension*)p;
            p += (1 + len);
        }
        else {
            p++;
        }

        while ((p < ext_end) && (*p == 0))
        {
            p++;
        }
    }
}

void RtpPacket::ParseTwobytesExt() {
    twobytes_ext_map_.clear();

    uint8_t* ext_start = (uint8_t*)(ext_) + 4;//skip id(16bits) + length(16bits)
    uint8_t* ext_end   = ext_start + GetExtLength(ext_);
    uint8_t* p         = ext_start;

/*
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |       0x10    |    0x00       |           length=3            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |      ID       |     L=0       |     ID        |     L=1       |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |       data    |    0 (pad)    |       ID      |      L=4      |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          data                                 |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    The 8-bit length field is the length of extension data in bytes not
    including the ID and length fields.  The value zero indicates there
    is no data following.
*/
    while (p + 1 < ext_end) {
        uint8_t id  = *p;
        uint8_t len = *(p + 1);

        if (id != 0) {
            if (p + 2 + len > ext_end) {
                CSM_THROW_ERROR("rtp extension length(%d) is not enough in two bytes extension mode.",
                    GetExtLength(ext_));
                break;
            }

            // Store the Two-Bytes extension element in the map.
            twobytes_ext_map_[id] = (TwobytesExtension*)p;

            p += (2 + len);
        } else {
            ++p;
        }

        while ((p < ext_end) && (*p == 0)) {
            ++p;
        }
    }
}

bool RtpPacket::HasOnebyteExt(HeaderExtension* rtp_ext) {
    return GetExtId(rtp_ext) == 0xBEDE;
}

bool RtpPacket::HasTwobytesExt(HeaderExtension* rtp_ext) {
/*
       0                   1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |         0x100         |appbits|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
    return (GetExtId(rtp_ext) & 0xfff0) == 0x1000;
}

uint8_t* RtpPacket::GetExtension(uint8_t id, uint8_t& len) {
    if (HasOnebyteExt(ext_)) {
        auto iter = onebyte_ext_map_.find(id);
        if (iter == onebyte_ext_map_.end()) {
            return nullptr;
        }

        OnebyteExtension* ext_data = iter->second;
        len = ext_data->len + 1;
        return ext_data->value;
    } else if (HasTwobytesExt(ext_)) {
        auto iter = twobytes_ext_map_.find(id);
        if (iter == twobytes_ext_map_.end()) {
            return nullptr;
        }
        TwobytesExtension* ext_data = iter->second;
        len = ext_data->len;
        if (len == 0) {
            return nullptr;
        }
        return ext_data->value;
    } else {
        //LogErrorf(logger_, "the extension bytes type is wrong.")
        return nullptr;
    }
}

bool RtpPacket::UpdateMid(uint8_t mid) {
    uint8_t extern_len = 0;
    uint8_t* extern_value = GetExtension(mid_extension_id_, extern_len);

    if (extern_value == nullptr) {
        LogErrorf(logger_, "The rtp packet has not extern mid:%d", mid_extension_id_);
        return false;
    }

    std::string mid_str = std::to_string(mid);

    memcpy(extern_value, mid_str.c_str(), mid_str.length());

    //update extension length
    return UpdateExtensionLength(mid_extension_id_, mid_str.length());
}

bool RtpPacket::ReadMid(uint8_t& mid) {
    uint8_t extern_len = 0;
    uint8_t* extern_value = GetExtension(mid_extension_id_, extern_len);

    if (extern_value == nullptr) {
        LogErrorf(logger_, "The rtp packet has not extern mid:%d", mid_extension_id_);
        return false;
    }
    std::string mid_str((char*)extern_value, extern_len);
    mid = (uint8_t)atoi(mid_str.c_str());

    return true;
}

bool RtpPacket::UpdateTransportWideSeq(uint16_t seq) {
    uint8_t extern_len = 0;
    uint8_t* extern_value = GetExtension(transport_wideCc_extension_id_, extern_len);

    if (extern_value == nullptr) {
        LogErrorf(logger_, "The rtp packet has not extern transport wideCc id:%d", transport_wideCc_extension_id_);
        return false;
    }

    if (extern_len != 2) {
        LogWarnf(logger_, "update extern transport wideCc length is not 2, extern_len:%d", extern_len);
    }
    ByteStream::Write2Bytes(extern_value, seq);

    //update extension length
    return UpdateExtensionLength(transport_wideCc_extension_id_, 3);
}

bool RtpPacket::GetTransportWideSeq(uint16_t& seq) {
    uint8_t extern_len = 0;
    uint8_t* extern_value = GetExtension(transport_wideCc_extension_id_, extern_len);

    if (extern_value == nullptr) {
        //LogErrorf(logger_, "The rtp packet has not extern transport wideCc id:%d", transport_wideCc_extension_id_);
        return false;
    }

    if (extern_len != 2) {
        LogWarnf(logger_, "read transport wideCc length is not 2, extern_len:%d", extern_len);
    }
    seq = ByteStream::Read2Bytes(extern_value);

    return true;
}

bool RtpPacket::ReadAbsTime(uint32_t& abs_time_24bits) {
    uint8_t extern_len = 0;
    uint8_t* extern_value = GetExtension(abs_time_extension_id_, extern_len);

    if (extern_value == nullptr) {
        //LogErrorf(logger_, "The rtp packet has not extern abs time id:%d", abs_time_extension_id_);
        return false;
    }

    if (extern_len != 3) {
        LogWarnf(logger_, "read abs time length is not 3, extern_len:%d", extern_len);
    }
    abs_time_24bits = ByteStream::Read3Bytes(extern_value);

    return true;
}

bool RtpPacket::UpdateAbsTime(uint32_t abs_time_24bits) {
    uint8_t extern_len = 0;
    uint8_t* extern_value = GetExtension(abs_time_extension_id_, extern_len);

    if (extern_value == nullptr) {
        LogErrorf(logger_, "The rtp packet has not extern abs time id:%d", abs_time_extension_id_);
        return false;
    }

    if (extern_len != 3) {
        LogWarnf(logger_, "update abs time length is not 3, extern_len:%d", extern_len);
    }
    ByteStream::Write3Bytes(extern_value, abs_time_24bits);

    //update extension length
    return UpdateExtensionLength(abs_time_extension_id_, 3);
}

bool RtpPacket::UpdateExtensionLength(uint8_t id, uint8_t len) {
    if (len == 0) {
        LogErrorf(logger_, "update extension length error: len must not be zero.");
        return false;
    }
    if (HasOnebyteExt(ext_)) {
        auto iter = onebyte_ext_map_.find(id);
        if (iter == onebyte_ext_map_.end()) {
            LogErrorf(logger_, "fail to get id:%d from the onebyte ext map.", id);
            return false;
        }
        auto* extension = iter->second;
        uint8_t current_len = extension->len + 1;
        if (len < current_len) {
            memset(extension->value + len, 0, current_len - len);
        }
        extension->len = len - 1;
    } else if (HasTwobytesExt(ext_)) {
        auto iter = twobytes_ext_map_.find(id);
        if (iter == twobytes_ext_map_.end()) {
            LogErrorf(logger_, "fail to get id:%d from the twobytes ext map.", id);
            return false;
        }
        auto* extension = iter->second;
        uint8_t current_len = extension->len;
        if (len < current_len) {
            memset(extension->value + len, 0, current_len - len);
        }
        extension->len = len;
    } else {
        LogErrorf(logger_, "the extension bytes type is wrong.");
        return false;
    }
    return true;
}

void RtpPacket::RtxDemux(uint32_t ssrc, uint8_t payloadtype) {
    if (payload_len_ < 2) {
        CSM_THROW_ERROR("rtx payload len(%lu) is less than 2", payload_len_);
    }

    uint16_t replace_seq = ntohs(*(uint16_t*)(payload_));
    SetPayloadType(payloadtype);
    SetSeq(replace_seq);
    SetSsrc(ssrc);

    std::memmove(payload_, payload_ + 2, payload_len_ - 2);
    payload_len_ -= 2;
    data_len_    -= 2;

    if (HasPadding()) {
        SetPadding(false);
        data_len_ -= pad_len_;
        pad_len_   = 0;
    }
}

void RtpPacket::RtxMux(uint8_t payload_type, uint32_t ssrc, uint16_t seq) {
    SetPayloadType(payload_type);
    SetSsrc(ssrc);
    
    std::memmove(payload_ + 2, payload_, payload_len_);
    ByteStream::Write2Bytes(payload_, GetSeq());

    SetSeq(seq);

    payload_len_ += 2u;
    data_len_ += 2u;

    //remove padding 
    if (HasPadding())
    {
        SetPadding(false);

        data_len_ -= pad_len_;
        pad_len_ = 0;
    }
}
}

