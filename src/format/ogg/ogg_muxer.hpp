#ifndef OGG_MUXER_HPP
#define OGG_MUXER_HPP
#include "utils/logger.hpp"
#include "format/opus_header.hpp"
#include "ogg_pub.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>

namespace cpp_streamer
{

#define OGG_NULL_HEADER_TYPE        0x00
#define OGG_CONTINUE_HEADER_TYPE    0x01
#define OGG_FIRST_HEADER_TYPE       0x02
#define OGG_LAST_HEADER_TYPE        0x04

class OggMuxer
{
private:
    OggPacketCallbackI* cb_ = nullptr;
    Logger* logger_         = nullptr;

private:
    bool first_pkt_         = true;
    uint32_t pg_seq_num_    = 0;

public:
    OggMuxer(OggPacketCallbackI* cb, Logger* logger);
    ~OggMuxer();

public:
    static uint32_t GetOggsUint32();
    int InputPacket(const uint8_t* data, size_t len, int64_t dts, int channel, int sample_rate);

private:
    void GenPageHeader(uint8_t header_type_flag, int channel, int sample_rate, uint32_t seq, OGG_PAGE_HEADER& pg_header);
    int GenExtraData(int64_t dts, int channel, int sample_rate);
};

}
#endif