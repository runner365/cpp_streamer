#ifndef OPUS_HEADER_HPP
#define OPUS_HEADER_HPP
#include <stdint.h>
#include <stddef.h>
#include <vector>

namespace cpp_streamer
{

bool GetOpusFrameVector(uint8_t* data, int len, std::vector<std::pair<uint8_t*, int>>& frames_vec);

class OpusExtraHandler
{
public:
    OpusExtraHandler();
    ~OpusExtraHandler();

public:
    static bool IsExtraData(const uint8_t* data, size_t len);
    static bool GenOpusExtraData(int clock_rate, int channel, uint8_t* extra_data, size_t& extra_len);

public:
    int DemuxExtraData(const uint8_t* data, size_t len);
    std::string DumpExtraData();

public:
    uint8_t version_    = 0;//data[8];
    uint8_t channel_    = 0;//data[9];
    uint16_t delay_     = 0;//data[10..11]
    uint32_t samperate_ = 0;//data[12..15]
    uint16_t gain_      = 0;//data[16..17]
    uint8_t map_type_   = 0;//data[18]

private:
    std::vector<uint8_t> extra_data_;
};

}
#endif
