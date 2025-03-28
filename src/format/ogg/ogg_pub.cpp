#include "ogg_pub.hpp"
#include "logger.hpp"

using json = nlohmann::json;

namespace cpp_streamer
{

OGG_PAGE_HEADER::OGG_PAGE_HEADER()
{
    strncpy(Oggs, "OggS", 4);
    ver = 0;
    header_type_flag = OGG_NULL_HEADER_TYPE;
    granule_position = 0;
    stream_serial_num = 0;
    page_sequence_number = 0;
    CRC_checksum = 0;
    seg_num = 0;
}

OGG_PAGE_HEADER::~OGG_PAGE_HEADER()
{
}


OGG_PAGE_HEADER* OGG_PAGE_HEADER::Parse(const uint8_t* data, size_t len) {
    OGG_PAGE_HEADER* header = new OGG_PAGE_HEADER();

    if (data[0] != 'O' || data[1] != 'g' || data[2] != 'g' || data[3] != 'S') {
        return nullptr;
    }
    const uint8_t* p = data;
    memcpy(header->Oggs, p, 4);
    p += 4;
    header->ver = *p;
    p++;
    header->header_type_flag = *p;
    p++;
    header->granule_position = ByteStream::Read8BytesLe(p);
    p += 8;
    header->stream_serial_num = ByteStream::Read4BytesLe(p);
    p += 4;
    header->page_sequence_number = ByteStream::Read4BytesLe(p);
    p += 4;
    header->CRC_checksum = ByteStream::Read4BytesLe(p);
    p += 4;
    header->seg_num = *p;

    return header;
}

void OGG_PAGE_HEADER::Sererialize(uint8_t* data, size_t& len) {
    uint8_t* p = data;

    memcpy(p, Oggs, 4);
    p += 4;
    *p = ver;
    p++;
    *p = header_type_flag;
    p++;
    ByteStream::Write8Bytes_le(p, granule_position);
    p += 8;
    ByteStream::Write4Bytes_le(p, stream_serial_num);
    p += 4;
    ByteStream::Write4Bytes_le(p, page_sequence_number);
    p += 4;
    ByteStream::Write4Bytes_le(p, 0);
    p += 4;
    *p = seg_num;
    p++;

    len = p - data;
    return;
}

json OGG_PAGE_HEADER::Dump() {
    std::stringstream ss;
    json data_json;
    for (size_t i = 0; i < sizeof(Oggs); i++) {
        ss << Oggs[i];
    }
    data_json["Oggs"] = ss.str();
    ss.clear();
    data_json["ver"] = (int)ver;
    data_json["header_type_flag"] = (int)header_type_flag;
    data_json["granule_position_array"] = granule_position;
    data_json["stream_serial_num"] = stream_serial_num;

    data_json["page_sequence_number"] = page_sequence_number;
    data_json["CRC_checksum"] = CRC_checksum;
    data_json["seg_num"] = (int)seg_num;

    return data_json;
}

OggPage::OggPage()
{
}

OggPage::~OggPage() 
{
    if (header_) {
        delete header_;
        header_ = nullptr;
    }
}

std::string OggPage::Dump() {
    if (header_ == nullptr) {
        return "";
    }
    json data_json = header_->Dump();

    data_json["seg_size_array"] = json::array();
    auto seg_size_array_iter = data_json.find("seg_size_array");
    for(size_t i = 0; i < segs_size_.size(); i++) {
        seg_size_array_iter->push_back((int)segs_size_[i]);
    }
    return data_json.dump();
}

}