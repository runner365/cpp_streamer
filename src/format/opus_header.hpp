#ifndef OPUS_HEADER_HPP
#define OPUS_HEADER_HPP
#include <stdint.h>
#include <stddef.h>
#include <vector>

namespace cpp_streamer
{

bool GetOpusFrameVector(uint8_t* data, int len, std::vector<std::pair<uint8_t*, int>>& frames_vec);

bool GetOpusExtraData(int clock_rate, int channel, uint8_t* extra_data, size_t& extra_len);

}
#endif
