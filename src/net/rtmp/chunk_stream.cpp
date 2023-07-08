#include "chunk_stream.hpp"
#include "rtmp_session_base.hpp"
//#include "rtmp_server_session.hpp"
#include "rtmp_pub.hpp"
#include "logger.hpp"

namespace cpp_streamer
{

static const int FORMAT0_HEADER_LEN = 11;
static const int FORMAT1_HEADER_LEN = 7;
static const int FORMAT2_HEADER_LEN = 3;
static const int EXT_TS_LEN = 4;

ChunkStream::ChunkStream(RtmpSessionBase* session, 
        uint8_t fmt, 
        uint16_t csid, 
        uint32_t chunk_size,
        Logger* logger):logger_(logger) {
    session_ = session;
    fmt_     = fmt;
    csid_    = csid;
    chunk_size_     = chunk_size;
    msg_count_      = 0;
    chunk_all_ptr_  = std::make_shared<DataBuffer>(50*1024);
    chunk_data_ptr_ = std::make_shared<DataBuffer>(50*1024);
}

ChunkStream::~ChunkStream() {

}

bool ChunkStream::IsReady() {
    return chunk_ready_;
}

int ChunkStream::GenSetRecordedMessage() {
    uint8_t data[6];

    fmt_           = 0;
    csid_          = 2;
    type_id_       = (uint8_t)RTMP_CONTROL_USER_CTRL_MESSAGES;
    msg_stream_id_ = 1;
    msg_len_       = 6;

    data[0] = (((uint8_t)Event_streamIsRecorded) >> 8) & 0xff;
    data[1] = ((uint8_t)Event_streamIsRecorded) & 0xff;

    for (int i = 0; i < 4; i++) {
        data[2+i] = (1 >> (uint32_t)((3-i)*8)) & 0xff;
    }

    GenData(data, 6);

    return RTMP_OK;
}

int ChunkStream::GenSetBeginMessage() {
    uint8_t data[6];

    fmt_           = 0;
    csid_          = 2;
    type_id_       = (uint8_t)RTMP_CONTROL_USER_CTRL_MESSAGES;
    msg_stream_id_ = 1;
    msg_len_       = 6;

    data[0] = (((uint8_t)Event_streamBegin) >> 8) & 0xff;
    data[1] = ((uint8_t)Event_streamBegin) & 0xff;

    for (int i = 0; i < 4; i++) {
        data[2+i] = (1 >> (uint32_t)((3-i)*8)) & 0xff;
    }

    GenData(data, 6);

    return RTMP_OK;
}

int ChunkStream::GenControlMessage(RTMP_CONTROL_TYPE ctrl_type, uint32_t size, uint32_t value) {
    fmt_           = 0;
    type_id_       = (uint8_t)ctrl_type;
    msg_stream_id_ = 0;
    msg_len_       = size;

    uint8_t* data = new uint8_t[size];
    switch (size) {
        case 1:
        {
            data[0] = (uint8_t)value;
            break;
        }
        case 2:
        {
            ByteStream::Write2Bytes(data, value);
            break;
        }
        case 3:
        {
            ByteStream::Write3Bytes(data, value);
            break;
        }
        case 4:
        {
            ByteStream::Write4Bytes(data, value);
            break;
        }
        case 5:
        {
            ByteStream::Write4Bytes(data, value);
            data[4] = 2;
            break;
        }
        default:
        {
            LogErrorf(logger_, "unkown control size:%u", size);
            delete[] data;
            return -1;
        }
    }

    GenData(data, size);

    delete[] data;
    return RTMP_OK;
}

int ChunkStream::ReadMsgFormat0(uint8_t input_fmt, uint16_t input_csid) {
    DataBuffer* buffer_p = session_->GetRecvBuffer();

    if (!buffer_p->Require(FORMAT0_HEADER_LEN)) {
        return RTMP_NEED_READ_MORE;
    }
    
    fmt_  = input_fmt;
    csid_ = input_csid;

    uint8_t* p  = (uint8_t*)buffer_p->Data();
    uint32_t ts = ByteStream::Read3Bytes(p);
    if (ts >= 0xffffff) {
        //require 4 bytes
        ext_ts_flag_ = true;
        if (!buffer_p->Require(FORMAT0_HEADER_LEN + EXT_TS_LEN)) {
            return RTMP_NEED_READ_MORE;
        }
    }

    p += 3;
    uint32_t msg_len = ByteStream::Read3Bytes(p);
    p += 3;
    uint8_t type_id = *p;
    p++;
    uint32_t msg_stream_id = ByteStream::Read4Bytes(p);
    p += 4;

    buffer_p->ConsumeData(FORMAT0_HEADER_LEN);

    if (ts >= 0xffffff) {
        ts = ByteStream::Read4Bytes(p);
        buffer_p->ConsumeData(EXT_TS_LEN);
    }

    timestamp32_     = ts;
    timestamp_delta_ = 0;

    msg_len_ = msg_len;
    type_id_ = type_id;
    msg_stream_id_ = msg_stream_id;

    remain_  = msg_len_;

    return RTMP_OK;
}

int ChunkStream::ReadMsgFormat1(uint8_t input_fmt, uint16_t input_csid) {
    DataBuffer* buffer_p = session_->GetRecvBuffer();

    if (!buffer_p->Require(FORMAT1_HEADER_LEN)) {
        return RTMP_NEED_READ_MORE;
    }
    
    fmt_  = input_fmt;
    csid_ = input_csid;

    uint8_t* p  = (uint8_t*)buffer_p->Data();
    uint32_t ts =  ByteStream::Read3Bytes(p);
    if (ts >= 0xffffff) {
        //require 4 bytes
        ext_ts_flag_ = true;
        if (!buffer_p->Require(FORMAT1_HEADER_LEN + EXT_TS_LEN)) {
            return RTMP_NEED_READ_MORE;
        }
    }

    p += 3;
    uint32_t msg_len = ByteStream::Read3Bytes(p);
    p += 3;
    uint8_t type_id = *p;
    p++;

    buffer_p->ConsumeData(FORMAT1_HEADER_LEN);

    if (ts >= 0xffffff) {
        ts = ByteStream::Read4Bytes(p);
        buffer_p->ConsumeData(EXT_TS_LEN);
    }

    timestamp_delta_ = ts;
    timestamp32_    += ts;

    msg_len_ = msg_len;
    type_id_ = type_id;

    remain_  = msg_len_;
    return RTMP_OK;
}

int ChunkStream::ReadMsgFormat2(uint8_t input_fmt, uint16_t input_csid) {
    DataBuffer* buffer_p = session_->GetRecvBuffer();

    if (!buffer_p->Require(FORMAT2_HEADER_LEN)) {
        return RTMP_NEED_READ_MORE;
    }

    fmt_  = input_fmt;
    csid_ = input_csid;

    uint8_t* p  = (uint8_t*)buffer_p->Data();
    uint32_t ts =  ByteStream::Read3Bytes(p);
    if (ts >= 0xffffff) {
        //require 4 bytes more
        ext_ts_flag_ = true;
        if (!buffer_p->Require(FORMAT2_HEADER_LEN + EXT_TS_LEN)) {
            return RTMP_NEED_READ_MORE;
        }
    }

    p += 3;

    buffer_p->ConsumeData(FORMAT2_HEADER_LEN);

    if (ts >= 0xffffff) {
        ts = ByteStream::Read4Bytes(p);
        buffer_p->ConsumeData(EXT_TS_LEN);
    }

    timestamp_delta_ = ts;
    timestamp32_    += ts;

    remain_  = msg_len_;
    return RTMP_OK;
}

int ChunkStream::ReadMsgFormat3(uint8_t input_fmt, uint16_t input_csid) {
    DataBuffer* buffer_p = session_->GetRecvBuffer();
    uint8_t* p;

    if (remain_ == 0) {
        //it's a new one
        switch (fmt_) {
            case 0:
            {
                if (ext_ts_flag_) {
                    if (!buffer_p->Require(EXT_TS_LEN)) {
                        return RTMP_NEED_READ_MORE;
                    }
                    p = (uint8_t*)buffer_p->Data();
                    timestamp32_ = ByteStream::Read4Bytes(p);
                    buffer_p->ConsumeData(EXT_TS_LEN);
                }
                break;
            }
            case 1:
            case 2:
            {
                uint32_t ts_delta = 0;
                if (ext_ts_flag_) {
                    if (!buffer_p->Require(EXT_TS_LEN)) {
                        return RTMP_NEED_READ_MORE;
                    }
                    p = (uint8_t*)buffer_p->Data();
                    ts_delta = ByteStream::Read4Bytes(p);
                    buffer_p->ConsumeData(EXT_TS_LEN);
                } else {
                    ts_delta = timestamp_delta_;
                }
                timestamp32_ += ts_delta;
                break;
            }
        }
        //chunk stream data reset
        chunk_all_ptr_->Reset();
        chunk_data_ptr_->Reset();
        remain_      = msg_len_;
        chunk_ready_ = false;
    } else {
        if (ext_ts_flag_) {
            if (!buffer_p->Require(EXT_TS_LEN)) {
                return RTMP_NEED_READ_MORE;
            }
            p = (uint8_t*)buffer_p->Data();
            uint32_t ts = ByteStream::Read4Bytes(p);
            if (ts == timestamp32_) {
                buffer_p->ConsumeData(EXT_TS_LEN);
            }
        }
    }
    fmt_  = input_fmt;
    csid_ = input_csid;

    return RTMP_OK;
}

int ChunkStream::ReadMessageHeader(uint8_t input_fmt, uint16_t input_csid) {
    int ret = -1;

    if (phase_ != CHUNK_STREAM_PHASE_HEADER) {
        return RTMP_OK;
    }

    if (input_fmt == 0) {
        ret = ReadMsgFormat0(input_fmt, input_csid);
    } else if (input_fmt == 1) {
        ret = ReadMsgFormat1(input_fmt, input_csid);
    } else if (input_fmt == 2) {
        ret = ReadMsgFormat2(input_fmt, input_csid);
    } else if (input_fmt == 3) {
        ret = ReadMsgFormat3(input_fmt, input_csid);
    } else {
        assert(0);
        return -1;
    }

    if (ret != RTMP_OK) {
        return ret;
    }

    msg_count_ = 0;
    phase_ = CHUNK_STREAM_PHASE_PAYLOAD;
    return RTMP_OK;
}

int ChunkStream::ReadMessagePayload() {
    if (phase_ != CHUNK_STREAM_PHASE_PAYLOAD) {
        return RTMP_OK;
    }

    DataBuffer* buffer_p = session_->GetRecvBuffer();
    require_len_ = (remain_ > chunk_size_) ? chunk_size_ : remain_;
    if (!buffer_p->Require(require_len_)) {
        return RTMP_NEED_READ_MORE;
    }
    chunk_data_ptr_->AppendData(buffer_p->Data(), require_len_);
    buffer_p->ConsumeData(require_len_);

    remain_ -= require_len_;
    msg_count_++;

    if (remain_ <= 0) {
        chunk_ready_ = true;
    }

    phase_ = CHUNK_STREAM_PHASE_HEADER;
    return RTMP_OK;
}

void ChunkStream::DumpHeader() {
    DataBuffer* buffer_p = session_->GetRecvBuffer();
    LogInfof(logger_, "basic header: fmt=%d, csid:%d; message header: timestamp=%lu, \
msglen=%u, typeid:%d, msg streamid:%u, remain:%ld, recv buffer len:%lu, chunk_size_:%u",
        fmt_, csid_, timestamp32_, msg_len_, type_id_, msg_stream_id_, remain_,
        buffer_p->DataLen(), chunk_size_);
}

void ChunkStream::DumpPayload() {
    char desc[128];

    snprintf(desc, sizeof(desc), "chunk stream payload:%lu", chunk_data_ptr_->DataLen());
    LogInfoData(logger_, (uint8_t*)chunk_data_ptr_->Data(), chunk_data_ptr_->DataLen(), desc);
}

void ChunkStream::DumpAllData() {
    char desc[128];

    snprintf(desc, sizeof(desc), "chunk stream all data:%lu", chunk_all_ptr_->DataLen());
    LogInfoData(logger_, (uint8_t*)chunk_all_ptr_->Data(), chunk_all_ptr_->DataLen(), desc);
}

int ChunkStream::GenData(uint8_t* data, int len) {
    if (csid_ < 64) {
        uint8_t fmt_csid_data[1];
        fmt_csid_data[0] = (fmt_ << 6) | (csid_ & 0x3f);
        chunk_all_ptr_->AppendData((char*)fmt_csid_data, sizeof(fmt_csid_data));
    } else if ((csid_ - 64) < 256) {
        uint8_t fmt_csid_data[2];
        fmt_csid_data[0] = (fmt_ << 6) & 0xc0;
        fmt_csid_data[1] = csid_;
        chunk_all_ptr_->AppendData((char*)fmt_csid_data, sizeof(fmt_csid_data));
    } else if ((csid_ - 64) < 65536) {
        uint8_t fmt_csid_data[3];
        fmt_csid_data[0] = ((fmt_ << 6) & 0xc0) | 0x01;
        ByteStream::Write2Bytes(fmt_csid_data + 1, csid_);
        chunk_all_ptr_->AppendData((char*)fmt_csid_data, sizeof(fmt_csid_data));
    } else {
        LogErrorf(logger_, "csid error:%d", csid_);
        return -1;
    }

    if (fmt_ == 0) {
        uint8_t header_data[11];
        uint8_t* p = header_data;
        ByteStream::Write3Bytes(p, timestamp32_);
        p += 3;
        ByteStream::Write3Bytes(p, msg_len_);
        p += 3;
        *p = type_id_;
        p++;
        ByteStream::Write4Bytes(p, msg_stream_id_);
        chunk_all_ptr_->AppendData((char*)header_data, sizeof(header_data));
    } else if (fmt_ == 1) {
        uint8_t header_data[7];
        uint8_t* p = header_data;
        ByteStream::Write3Bytes(p, timestamp32_);
        p += 3;
        ByteStream::Write3Bytes(p, msg_len_);
        p += 3;
        *p = type_id_;
        chunk_all_ptr_->AppendData((char*)header_data, sizeof(header_data));
    } else if (fmt_ == 2) {
        uint8_t header_data[3];
        uint8_t* p = header_data;
        ByteStream::Write3Bytes(p, timestamp32_);
        chunk_all_ptr_->AppendData((char*)header_data, sizeof(header_data));
    } else {
        //need do nothing
    }

    chunk_all_ptr_->AppendData((char*)data, (size_t)len);
    return RTMP_OK;
}

void ChunkStream::Reset() {
    phase_ = CHUNK_STREAM_PHASE_HEADER;
    chunk_ready_ = false;
    chunk_all_ptr_->Reset();
    chunk_data_ptr_->Reset();
}

int WriteDataByChunkStream(RtmpSessionBase* session, uint16_t csid,
                    uint32_t timestamp, uint8_t type_id,
                    uint32_t msg_stream_id, uint32_t chunk_size,
                    DataBuffer& input_buffer,
                    Logger* logger)
{
    int cs_count = input_buffer.DataLen()/chunk_size;
    int fmt = 0;
    uint8_t* p;
    int len;

    if ((input_buffer.DataLen()%chunk_size) > 0) {
        cs_count++;
    }

    for (int index = 0; index < cs_count; index++) {
        if (index == 0) {
            fmt = 0;
        } else {
            fmt = 3;
        }
        p = (uint8_t*)input_buffer.Data() + index * chunk_size;
        if (index == (cs_count-1)) {
            if ((input_buffer.DataLen()%chunk_size) > 0) {
                len = input_buffer.DataLen()%chunk_size;
            } else {
                len = chunk_size;
            }
        } else {
            len = chunk_size;
        }

        ChunkStream* c = new ChunkStream(session, fmt, csid, chunk_size, logger);

        c->timestamp32_ = timestamp;
        c->msg_len_     = input_buffer.DataLen();
        c->type_id_     = type_id;
        c->msg_stream_id_ = msg_stream_id;
        c->GenData(p, len);
        
        session->RtmpSend(c->chunk_all_ptr_);

        delete c;
    }
    return RTMP_OK;
}

int WriteDataByChunkStream(RtmpSessionBase* session, uint16_t csid,
                    uint32_t timestamp, uint8_t type_id,
                    uint32_t msg_stream_id, uint32_t chunk_size,
                    std::shared_ptr<DataBuffer> input_buffer_ptr,
                    Logger* logger)
{
    int cs_count = input_buffer_ptr->DataLen()/chunk_size;
    int fmt = 0;
    uint8_t* p;
    int len;

    if ((input_buffer_ptr->DataLen()%chunk_size) > 0) {
        cs_count++;
    }

    for (int index = 0; index < cs_count; index++) {
        if (index == 0) {
            fmt = 0;
        } else {
            fmt = 3;
        }
        p = (uint8_t*)input_buffer_ptr->Data() + index * chunk_size;
        if (index == (cs_count-1)) {
            if ((input_buffer_ptr->DataLen()%chunk_size) > 0) {
                len = input_buffer_ptr->DataLen()%chunk_size;
            } else {
                len = chunk_size;
            }
        } else {
            len = chunk_size;
        }
        ChunkStream* c = new ChunkStream(session, fmt, csid, chunk_size, logger);

        c->timestamp32_ = timestamp;
        c->msg_len_     = input_buffer_ptr->DataLen();
        c->type_id_     = type_id;
        c->msg_stream_id_ = msg_stream_id;
        c->GenData(p, len);
        
        //c->DumpHeader();
        //c->DumpAllData();
        session->RtmpSend(c->chunk_all_ptr_);

        delete c;
    }
    return RTMP_OK;
}

}

