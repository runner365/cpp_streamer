#ifndef AUDIO_HEADER_HPP
#define AUDIO_HEADER_HPP
#include "utils/byte_stream.hpp"
#include "utils/av/av.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <iostream>

namespace cpp_streamer
{
/* bitmasks to isolate specific values */
#define FLV_AUDIO_CHANNEL_MASK    0x01
#define FLV_AUDIO_SAMPLESIZE_MASK 0x02
#define FLV_AUDIO_SAMPLERATE_MASK 0x0c
#define FLV_AUDIO_CODECID_MASK    0xf0

#define FLV_8bits_SAMPLE_SIZE  0
#define FLV_16bits_SAMPLE_SIZE 1

bool GetAudioInfoByFlvHeader(uint8_t flag,
                       MEDIA_CODEC_TYPE& codec_type, 
                       int& sample_rate, 
                       int& sample_size,
                       uint8_t& channel);
 
int GetAscTypeByAdtsType(int adts_type);
int GetAdtsTypeByAscType(int asc_type);

int GetSamplerateIndex(int samplerate);

//xxxx xyyy yzzz z000
//aac type==5(or 29): xxx xyyy yzzz zyyy yzzz z000
bool GetAudioInfoByAsc(uint8_t* data, size_t data_size,
                       uint8_t& audio_type, int& sample_rate,
                       uint8_t& channel);


bool GetAudioInfo2ByAsc(uint8_t* data, size_t data_size,
                        uint8_t& audio_type, int& sample_rate,
                        uint8_t& channel);


int MakeAdts(uint8_t* data, uint8_t object_type,
             int sample_rate, int channel, int full_frame_size, bool mpegts2 = true);

size_t MakeOpusHeader(uint8_t* data, int sample_rate, int channel);

}

#endif //AUDIO_HEADER_HPP

