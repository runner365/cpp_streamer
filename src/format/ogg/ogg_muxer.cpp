#include "ogg_muxer.hpp"
#include "format/opus_header.hpp"
#include "utils/byte_stream.hpp"
#include "utils/byte_crypto.hpp"
#include <vector>

namespace cpp_streamer
{

static const std::string oggS = "OggS";
static const uint8_t stream_serial_num_def[4] = {0x23, 0x49, 0x06, 0x11};

OggMuxer::OggMuxer(OggPacketCallbackI* cb, Logger* logger):cb_(cb)
                                                        , logger_(logger)
{
}

OggMuxer::~OggMuxer()
{
}

uint32_t OggMuxer::GetOggsUint32() {
    uint32_t ret = 0;

    ret += ((uint32_t)oggS[0]) << 24;
    ret += ((uint32_t)oggS[1]) << 16;
    ret += ((uint32_t)oggS[2]) << 8;
    ret += ((uint32_t)oggS[4]);
    return ret;
}

int OggMuxer::InputPacket(const uint8_t* data, size_t len, int64_t dts, int channel, int sample_rate) {
    if (first_pkt_) {
        first_pkt_ = false;
        int ret = GenExtraData(dts, channel, sample_rate);
        if (ret < 0) {
            LogErrorf(logger_, "generate extra data error");
            return ret;
        }
    }

    OGG_PAGE_HEADER pg_header;

    GenPageHeader(OGG_FIRST_HEADER_TYPE, channel, sample_rate, pg_seq_num_++, pg_header);

    size_t data_total = sizeof(OGG_PAGE_HEADER) + sizeof(uint8_t)*pg_header.seg_num + len;
    std::vector<uint8_t> ogg_data(data_total);
    uint8_t* p = &ogg_data[0];

    memcpy(p, &pg_header, sizeof(OGG_PAGE_HEADER));
    p += sizeof(OGG_PAGE_HEADER);
    *p = len;

    uint32_t crc = ByteCrypto::GetCrc32((uint8_t*)&ogg_data[0], p - &ogg_data[0]);
    ByteStream::Write4Bytes(pg_header.CRC_checksum, crc);
    memcpy(&ogg_data[0], &pg_header, sizeof(OGG_PAGE_HEADER));

    p++;
    memcpy(p, data, len);

    cb_->OnOggPacketCallback(&ogg_data[0], ogg_data.size(), dts);

    return 0;
}

void OggMuxer::GenPageHeader(uint8_t header_type_flag, int channel, int sample_rate, uint32_t seq, OGG_PAGE_HEADER& pg_header) {
    memset(&pg_header, 0, sizeof(OGG_PAGE_HEADER));

    memcpy(pg_header.Oggs, oggS.c_str(), oggS.length());
    pg_header.ver = 0;//default is 0
    pg_header.header_type_flag = header_type_flag;
    ByteStream::Write8Bytes(pg_header.granule_position, (uint64_t)sample_rate);
    memcpy(pg_header.stream_serial_num, stream_serial_num_def, sizeof(stream_serial_num_def));
    
    ByteStream::Write4Bytes(pg_header.page_sequence_number, pg_seq_num_++);
    pg_header.seg_num = 1;
    
    // uint32_t crc = ByteCrypto::GetCrc32((uint8_t*)&pg_header, 24);
    // ByteStream::Write4Bytes(pg_header.CRC_checksum, crc);

    return;
}

int OggMuxer::GenExtraData(int64_t dts, int channel, int sample_rate) {
    uint8_t extra_data[512];
    size_t extra_len = 0;

    bool ret = OpusExtraHandler::GenOpusExtraData(sample_rate, channel, extra_data, extra_len);
    if (!ret) {
        return -1;
    }

    OGG_PAGE_HEADER pg_header;

    GenPageHeader(OGG_FIRST_HEADER_TYPE, channel, sample_rate, pg_seq_num_++, pg_header);

    size_t data_total = sizeof(OGG_PAGE_HEADER) + sizeof(uint8_t)*pg_header.seg_num + extra_len;
    std::vector<uint8_t> data(data_total);
    uint8_t* p = &data[0];

    memcpy(p, &pg_header, sizeof(OGG_PAGE_HEADER));
    p += sizeof(OGG_PAGE_HEADER);
    *p = extra_len;

    uint32_t crc = ByteCrypto::GetCrc32((uint8_t*)&data[0], p - &data[0]);
    ByteStream::Write4Bytes(pg_header.CRC_checksum, crc);
    
    memcpy(&data[0], &pg_header, sizeof(OGG_PAGE_HEADER));

    p++;
    memcpy(p, extra_data, extra_len);

    cb_->OnOggPacketCallback(&data[0], data.size(), dts);
    return 0;
}
}