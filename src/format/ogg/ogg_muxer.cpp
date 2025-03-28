#include "ogg_muxer.hpp"
#include "format/opus_header.hpp"
#include "utils/byte_stream.hpp"
#include "utils/crc.hpp"
#include <vector>
#include <stdlib.h>

namespace cpp_streamer
{

OggMuxer::OggMuxer(OggPacketCallbackI* cb, Logger* logger):cb_(cb)
                                                        , logger_(logger)
{
    stream_serial_num_ = rand();
}

OggMuxer::~OggMuxer()
{
}

int OggMuxer::InputPacket(const uint8_t* data, size_t len, int64_t dts, int channel, int sample_rate) {
    if (first_pkt_) {
        first_pkt_ = false;
        int ret = GenExtraData(dts, channel, sample_rate);
        if (ret < 0) {
            LogErrorf(logger_, "generate extra data error");
            return ret;
        }
        MakeOpusConst(dts);
    }

    std::shared_ptr<OggItem> item_ptr = std::make_shared<OggItem>(data, len, dts, channel, sample_rate);
    ogg_items_.push_back(item_ptr);

    if (ogg_items_.size() < OGG_ITEM_MAX) {
        return 0;
    }
    granule_position_ += ogg_items_.size() * 20;
    OGG_PAGE_HEADER pg_header;
    pg_header.Oggs[0] = 'O';
    pg_header.Oggs[1] = 'g';
    pg_header.Oggs[2] = 'g';
    pg_header.Oggs[3] = 'S';
    pg_header.ver = 0;
    pg_header.header_type_flag = OGG_NULL_HEADER_TYPE;
    pg_header.granule_position = granule_position_;
    pg_header.stream_serial_num = stream_serial_num_;
    pg_header.page_sequence_number = pg_seq_num_++;
    pg_header.CRC_checksum = 0;
    pg_header.seg_num = (uint8_t)ogg_items_.size();

    uint8_t gp_header_data[OGG_PAGE_HEADER_SIZE];
    size_t gp_header_data_len = 0;
    pg_header.Sererialize(gp_header_data, gp_header_data_len);
    assert(gp_header_data_len == OGG_PAGE_HEADER_SIZE);

    size_t data_total = OGG_PAGE_HEADER_SIZE + ogg_items_.size();
    for (size_t i = 0; i < ogg_items_.size(); i++) {
        data_total += ogg_items_[i]->data.size();
    }
    
    LogDebugf(logger_, "page data total:%lu, page header:%s", data_total, pg_header.Dump().dump().c_str());
    std::vector<uint8_t> ogg_data(data_total);
    uint8_t* p = &ogg_data[0];
    uint8_t* crc_p = p + OGG_PAGE_HEADER_CRC_OFFSET;

    memcpy(p, gp_header_data, OGG_PAGE_HEADER_SIZE);
    p += OGG_PAGE_HEADER_SIZE;

    for (size_t i = 0; i < ogg_items_.size(); i++) {
        *p = (uint8_t)ogg_items_[i]->data.size();
        p++;
    }
    uint32_t crc = GetCrc32(CRC_32_IEEE, 0, (uint8_t*)&ogg_data[0], p - &ogg_data[0]);

    uint8_t* data_start = p;
    for (size_t i = 0; i < ogg_items_.size(); i++) {
        memcpy(p, &ogg_items_[i]->data[0], ogg_items_[i]->data.size());
        p += ogg_items_[i]->data.size();
    }

    crc = GetCrc32(CRC_32_IEEE, crc, data_start, p - data_start);
    ByteStream::Write4Bytes(crc_p, crc);
    ogg_items_.clear();

    cb_->OnOggPacketCallback(&ogg_data[0], data_total, dts);
    return 0;
}


int OggMuxer::GenExtraData(int64_t dts, int channel, int sample_rate) {
    uint8_t extra_data[512];
    size_t extra_len = 0;

    bool ret = OpusExtraHandler::GenOpusExtraData(sample_rate, channel, extra_data, extra_len);
    if (!ret) {
        return -1;
    }

    uint8_t pg_header_data[OGG_PAGE_HEADER_SIZE];
    size_t pg_header_data_len = 0;
    OGG_PAGE_HEADER pg_header;

    pg_header.Oggs[0] = 'O';
    pg_header.Oggs[1] = 'g';
    pg_header.Oggs[2] = 'g';
    pg_header.Oggs[3] = 'S';
    pg_header.ver = 0;
    pg_header.header_type_flag = OGG_FIRST_HEADER_TYPE;
    pg_header.granule_position = 0;
    pg_header.stream_serial_num = stream_serial_num_;
    pg_header.page_sequence_number = pg_seq_num_++;
    pg_header.CRC_checksum = 0;
    pg_header.seg_num = 1;

    pg_header.Sererialize(pg_header_data, pg_header_data_len);
    assert(pg_header_data_len == OGG_PAGE_HEADER_SIZE);

    size_t data_total = OGG_PAGE_HEADER_SIZE + pg_header.seg_num + extra_len;

    std::vector<uint8_t> data(data_total);
    uint8_t* p = &data[0];
    uint8_t* crc_p = p + OGG_PAGE_HEADER_CRC_OFFSET;

    memcpy(p, pg_header_data, OGG_PAGE_HEADER_SIZE);
    p += OGG_PAGE_HEADER_SIZE;
    *p = extra_len;
    p++;
    uint32_t crc = GetCrc32(CRC_32_IEEE, 0, (uint8_t*)&data[0], p - &data[0]);

    memcpy(p, extra_data, extra_len);
    
    crc = GetCrc32(CRC_32_IEEE, crc, extra_data, extra_len);

    ByteStream::Write4Bytes(crc_p, crc);

    cb_->OnOggPacketCallback(&data[0], data.size(), dts);
    return 0;
}

void OggMuxer::MakeOpusConst(int64_t dts) {
    const uint8_t ogg_const_header[] = {0x4f, 0x70, 0x75, 0x73, 
        0x54, 0x61, 0x67, 0x73, 
        0x0d, 0x00, 0x00, 0x00, 
        0x4c, 0x61, 0x76, 0x66,
        0x35, 0x38, 0x2e, 0x32,
        0x30, 0x2e, 0x31, 0x30,
        0x30, 0x01, 0x00, 0x00,
        0x00, 0x15, 0x00, 0x00,
        0x00, 0x65, 0x6e, 0x63,
        0x6f, 0x64, 0x65, 0x72,
        0x3d, 0x4c, 0x61, 0x76,
        0x66, 0x35, 0x38, 0x2e,
        0x32, 0x30, 0x2e, 0x31,
        0x30, 0x30};


    uint8_t pg_header_data[OGG_PAGE_HEADER_SIZE];
    size_t pg_header_data_len = 0;
    OGG_PAGE_HEADER pg_header;

    pg_header.Oggs[0] = 'O';
    pg_header.Oggs[1] = 'g';
    pg_header.Oggs[2] = 'g';
    pg_header.Oggs[3] = 'S';
    pg_header.ver = 0;
    pg_header.header_type_flag = OGG_NULL_HEADER_TYPE;
    pg_header.granule_position = 0;
    pg_header.stream_serial_num = stream_serial_num_;
    pg_header.page_sequence_number = pg_seq_num_++;
    pg_header.CRC_checksum = 0;
    pg_header.seg_num = 1;

    pg_header.Sererialize(pg_header_data, pg_header_data_len);
    assert(pg_header_data_len == OGG_PAGE_HEADER_SIZE);

    size_t data_total = OGG_PAGE_HEADER_SIZE + pg_header.seg_num + sizeof(ogg_const_header);

    std::vector<uint8_t> data(data_total);
    uint8_t* p = &data[0];
    uint8_t* crc_p = p + OGG_PAGE_HEADER_CRC_OFFSET;

    memcpy(p, pg_header_data, OGG_PAGE_HEADER_SIZE);
    p += OGG_PAGE_HEADER_SIZE;
    *p = sizeof(ogg_const_header);
    p++;
    uint32_t crc = GetCrc32(CRC_32_IEEE, 0, (uint8_t*)&data[0], p - &data[0]);

    memcpy(p, ogg_const_header, sizeof(ogg_const_header));
    
    crc = GetCrc32(CRC_32_IEEE, crc, ogg_const_header, sizeof(ogg_const_header));

    ByteStream::Write4Bytes(crc_p, crc);

    cb_->OnOggPacketCallback(&data[0], data.size(), dts);
}

}