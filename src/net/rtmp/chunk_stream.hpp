#ifndef CHUNK_STREAM_HPP
#define CHUNK_STREAM_HPP
#include "data_buffer.hpp"
#include "byte_stream.hpp"
#include "rtmp_pub.hpp"
#include "logger.hpp"

#include <stdint.h>
#include <memory>

namespace cpp_streamer
{

typedef enum {
    CHUNK_STREAM_PHASE_HEADER,
    CHUNK_STREAM_PHASE_PAYLOAD
} CHUNK_STREAM_PHASE;

class RtmpSessionBase;
class ChunkStream
{
public:
    ChunkStream(RtmpSessionBase* session,
            uint8_t fmt,
            uint16_t csid,
            uint32_t chunk_size,
            Logger* logger = nullptr);
    ~ChunkStream();

public:
    bool IsReady();
    int ReadMessageHeader(uint8_t input_fmt, uint16_t input_csid);
    int ReadMessagePayload();

    void DumpHeader();
    void DumpPayload();
    void DumpAllData();

    int GenControlMessage(RTMP_CONTROL_TYPE ctrl_type, uint32_t size, uint32_t value);
    int GenSetRecordedMessage();
    int GenSetBeginMessage();
    int GenData(uint8_t* data, int len);

    void Reset();

private:
    int ReadMsgFormat0(uint8_t input_fmt, uint16_t input_csid);
    int ReadMsgFormat1(uint8_t input_fmt, uint16_t input_csid);
    int ReadMsgFormat2(uint8_t input_fmt, uint16_t input_csid);
    int ReadMsgFormat3(uint8_t input_fmt, uint16_t input_csid);

public:
    uint32_t timestamp_delta_ = 0;
    uint32_t timestamp32_     = 0;
    uint32_t msg_len_         = 0;
    uint8_t  type_id_         = 0;
    uint32_t msg_stream_id_   = 0;
    int64_t  remain_          = 0;
    int64_t  require_len_     = 0;
    uint32_t chunk_size_      = CHUNK_DEF_SIZE;
    std::shared_ptr<DataBuffer> chunk_all_ptr_;
    std::shared_ptr<DataBuffer> chunk_data_ptr_;

private:
    RtmpSessionBase* session_ = nullptr;
    bool ext_ts_flag_ = false;
    uint8_t fmt_      = 0;
    uint16_t csid_    = 0;
    CHUNK_STREAM_PHASE phase_ = CHUNK_STREAM_PHASE_HEADER;
    bool chunk_ready_  = false;
    int64_t msg_count_ = 0;

private:
    Logger* logger_ = nullptr;
};

using CHUNK_STREAM_PTR = std::shared_ptr<ChunkStream>;

int WriteDataByChunkStream(RtmpSessionBase* session, uint16_t csid,
                    uint32_t timestamp, uint8_t type_id,
                    uint32_t msg_stream_id, uint32_t chunk_size,
                    std::shared_ptr<DataBuffer> input_buffer,
                    Logger* logger = nullptr);

int WriteDataByChunkStream(RtmpSessionBase* session, uint16_t csid,
                    uint32_t timestamp, uint8_t type_id,
                    uint32_t msg_stream_id, uint32_t chunk_size,
                    DataBuffer& input_buffer,
                    Logger* logger = nullptr);

}
#endif
