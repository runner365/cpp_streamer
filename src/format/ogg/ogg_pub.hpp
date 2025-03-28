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

using json = nlohmann::json;

namespace cpp_streamer
{
#define OGG_ITEM_MAX 255
#define OGG_PAGE_HEADER_SIZE 27
#define OGG_PAGE_HEADER_CRC_OFFSET 22

#define OGG_NULL_HEADER_TYPE        0x00
#define OGG_CONTINUE_HEADER_TYPE    0x01
#define OGG_FIRST_HEADER_TYPE       0x02
#define OGG_LAST_HEADER_TYPE        0x04

class OGG_PAGE_HEADER
{
public:
	OGG_PAGE_HEADER();
	~OGG_PAGE_HEADER();

public:
	static OGG_PAGE_HEADER* Parse(const uint8_t* data, size_t len);
	void Sererialize(uint8_t* data, size_t& len);
	json Dump();

public:
	char    Oggs[4];        
	uint8_t ver;
	uint8_t header_type_flag;
	uint64_t granule_position;
	uint32_t stream_serial_num;
	uint32_t page_sequence_number;
	uint32_t CRC_checksum;
	uint8_t seg_num;
};

class OggPage
{
public:
    OggPage();
    ~OggPage();

    std::string Dump();

public:
    OGG_PAGE_HEADER* header_ = nullptr;
	std::vector<uint8_t> segs_size_;
    std::vector<std::shared_ptr<DataBuffer>> seg_buffer_vec_;
};

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