#ifndef OGG_PUB_HPP
#define OGG_PUB_HPP
#include "utils/json.hpp"
#include "utils/byte_stream.hpp"
#include "utils/data_buffer.hpp"
#include <sstream>
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <memory>

namespace cpp_streamer
{
typedef struct PAGE_HEADER_S {  
	char    Oggs[4];        
	uint8_t ver;
	uint8_t header_type_flag;
	uint8_t granule_position[8];
	uint8_t stream_serial_num[4];
	uint8_t page_sequence_number[4];
	uint8_t CRC_checksum[4];
	uint8_t seg_num;
} OGG_PAGE_HEADER;

class OggPage
{
public:
    OggPage();
    ~OggPage();

    std::string Dump();

public:
    OGG_PAGE_HEADER header_;
    std::vector<uint8_t> seg_size_vec_;
    std::vector<std::shared_ptr<DataBuffer>> seg_buffer_vec_;
};

std::string DumpOggPacketHeader(const OGG_PAGE_HEADER& header);

class OpusDataCallbackI
{
public:
	virtual void OnOpusPacketCallBack(int channel, int sample_rate, const uint8_t* data, size_t len, int64_t dts) = 0;
};

class OggPacketCallbackI
{
public:
	virtual void OnOggPacketCallback(const uint8_t* data, size_t len, int64_t dts) = 0;
};

}
#endif