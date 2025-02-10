#include "ogg_pub.hpp"

using json = nlohmann::json;

namespace cpp_streamer
{
OggPage::OggPage()
{
    memset(&header_, 0, sizeof(OGG_PAGE_HEADER));
}

OggPage::~OggPage() 
{
}

std::string OggPage::Dump() {
    std::stringstream ss;
    json data_json;
    for (size_t i = 0; i < sizeof(header_.Oggs); i++) {
        ss << header_.Oggs[i];
    }
    data_json["Oggs"] = ss.str();
    ss.clear();
    data_json["ver"] = (int)header_.ver;
    data_json["header_type_flag"] = (int)header_.header_type_flag;
    data_json["granule_position_array"] = (uint64_t)ByteStream::Read8BytesLe((uint8_t*)header_.granule_position);
    data_json["stream_serial_num"] = json::array();
    auto ss_num_array_iter = data_json.find("stream_serial_num");
    for (size_t i = 0; i < sizeof(header_.stream_serial_num); i++) {
        ss_num_array_iter->push_back((int)header_.stream_serial_num[i]);
    }
    data_json["page_sequence_number"] = json::array();
    auto ps_num_array = data_json.find("page_sequence_number");
    for (size_t i = 0; i < sizeof(header_.page_sequence_number); i++) {
        ps_num_array->push_back((int)header_.page_sequence_number[i]);
    }
    data_json["CRC_checksum"] = (uint32_t)ByteStream::Read4Bytes((uint8_t*)header_.CRC_checksum);
    data_json["seg_num"] = (int)header_.seg_num;
    data_json["seg_size_array"] = json::array();
    auto seg_size_array_iter = data_json.find("seg_size_array");
    for(size_t i = 0; i < seg_size_vec_.size(); i++) {
        seg_size_array_iter->push_back((int)seg_size_vec_[i]);
    }
    return data_json.dump();
}
}