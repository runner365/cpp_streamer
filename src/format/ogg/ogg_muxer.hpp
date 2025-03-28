#ifndef OGG_MUXER_HPP
#define OGG_MUXER_HPP
#include "utils/logger.hpp"
#include "format/opus_header.hpp"
#include "ogg_pub.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <memory>

namespace cpp_streamer
{

class OggItem
{
public:
    std::vector<uint8_t> data;
    int64_t dts;
    int channel;
    int sample_rate;

public:
    OggItem(const uint8_t* data, size_t len, int64_t dts, int channel, int sample_rate)
    {
        this->data.resize(len);
        memcpy(&this->data[0], data, len);
        this->dts = dts;
        this->channel = channel;
        this->sample_rate = sample_rate;
    }
};

class OggMuxer
{
private:
    OggPacketCallbackI* cb_ = nullptr;
    Logger* logger_         = nullptr;

private:
    bool first_pkt_          = true;
    uint32_t pg_seq_num_     = 0;
    size_t granule_position_ = 0;
    uint32_t stream_serial_num_ = 0;
    std::vector<std::shared_ptr<OggItem>> ogg_items_;

public:
    OggMuxer(OggPacketCallbackI* cb, Logger* logger);
    ~OggMuxer();

public:
    int InputPacket(const uint8_t* data, size_t len, int64_t dts, int channel, int sample_rate);

private:
    int GenExtraData(int64_t dts, int channel, int sample_rate);
    void MakeOpusConst(int64_t dts);
};

}
#endif