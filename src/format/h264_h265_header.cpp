#include "h264_h265_header.hpp"
#include "byte_stream.hpp"
#include "stringex.hpp"

#include <sstream>
#include <iostream>

namespace cpp_streamer
{

void GetHevcHeader(uint8_t* data, Hevc_Header& header) {
    uint8_t* p = data;

    header.forbid = ((*p) >> 7) & 0x01;
    header.nalu_type = ((*p) >> 1) & 0x3f;
    header.layer_id = ((*p) & 0x01) << 6;
    p++;
    header.layer_id |= ((*p) & 0xf8) >> 3;
    header.tid = (*p) & 0x07;
}

std::string HevcHeaderDump(const Hevc_Header& header) {
    std::stringstream ss;

    ss << "{";
    ss << "\"forbid\":" << (int)header.forbid << ",";
    ss << "\"nalu_type\":" << (int)header.nalu_type << ",";
    ss << "\"layer_id\":" << (int)header.layer_id << ",";
    ss << "\"tid\":" << (int)header.tid;
    ss << "}";
    return ss.str();
}

bool AnnexB2Nalus(uint8_t* data, size_t len, std::vector<std::shared_ptr<DataBuffer>>& nalus) {
    if (len < 4) {
        return false;
    }
    std::vector<size_t> pos_vec;
    uint8_t* p = data;

    while (p < data + len) {
        size_t left_len = data + len - p;

        if (left_len >= 4) {
            if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
                pos_vec.push_back((size_t)p);
                p += 4;
                continue;
            }
        }
        if (left_len >= 3) {
            if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
                pos_vec.push_back((size_t)p);
                p += 3;
                continue;
            }
        }
        p++;
    }

    uint8_t* end_pos = data + len;
    for(size_t index = 0; index < pos_vec.size(); index++) {
        size_t current_pos = pos_vec[index];
        size_t nalu_len = 0;
        if ((index + 1) < pos_vec.size()) {
            nalu_len = pos_vec[index + 1] - pos_vec[index];
        } else {
            nalu_len = (size_t)end_pos - current_pos;
        }
        std::shared_ptr<DataBuffer> data_ptr = std::make_shared<DataBuffer>(nalu_len + 1024);
        data_ptr->AppendData((char*)current_pos, nalu_len);
        nalus.push_back(data_ptr);
    }
    return true;
}

bool AnnexB2Avcc(uint8_t* data, size_t len, std::vector<std::shared_ptr<DataBuffer>>& nalus) {
    if (len < 4) {
        return false;
    }
    std::vector<size_t> pos_vec;
    uint8_t* p = data;

    while (p < data + len) {
        size_t left_len = data + len - p;

        if (left_len >= 4) {
            if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
                p += 4;
                pos_vec.push_back((size_t)p);
                continue;
            }
        }
        if (left_len >= 3) {
            if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
                p += 3;
                pos_vec.push_back((size_t)p);
                continue;
            }
        }
        p++;
    }

    uint8_t* end_pos = data + len;
    for (size_t index = 0; index < pos_vec.size(); index++) {
        size_t current_pos = pos_vec[index];
        size_t next_pos  = (index + 1 != pos_vec.size()) ? pos_vec[index + 1] : (size_t)end_pos;

        size_t nalu_size = next_pos - current_pos;
        uint8_t header[4];

        ByteStream::Write4Bytes(header, nalu_size);

        std::shared_ptr<DataBuffer> buffer_ptr = std::make_shared<DataBuffer>(nalu_size + 1024);
        buffer_ptr->AppendData((char*)header, sizeof(header));
        buffer_ptr->AppendData((char*)current_pos, nalu_size);

        nalus.push_back(buffer_ptr);
    }
    return true;
}

bool Avcc2Nalus(uint8_t* data, size_t len, std::vector<std::shared_ptr<DataBuffer>>& nalus) {
    const uint32_t kMaxLen = 10*1000*1000;

    if (len < 4) {
        return false;
    }

    int64_t data_len = (int64_t)len;
    uint8_t* p = data;

    while(data_len > 0) {
        uint32_t nalu_len = ByteStream::Read4Bytes(p);
        if (nalu_len > kMaxLen) {
            return false;
        }
        p += 4;
        data_len -= 4;

        std::shared_ptr<DataBuffer> nalu_ptr = std::make_shared<DataBuffer>(4 + nalu_len + 1024);
        uint8_t nalu_data[4];
        nalu_data[0] = 0x00;
        nalu_data[1] = 0x00;
        nalu_data[2] = 0x00;
        nalu_data[3] = 0x01;

        nalu_ptr->AppendData((char*)nalu_data, sizeof(nalu_data));
        nalu_ptr->AppendData((char*)p, nalu_len);

        nalus.push_back(nalu_ptr);
        p += nalu_len;
        data_len -= nalu_len;
    }
    return true;
}

int GetSpsPpsFromExtraData(uint8_t *pps, size_t& pps_len, 
                           uint8_t *sps, size_t& sps_len, 
                           const uint8_t *extra_data, size_t extra_len)
{
    if (extra_len == 0) {
        return -1;
    }
    const unsigned char * body= nullptr;
    int iIndex = 0;
    
    body = extra_data;

    if(extra_len >4){
        if(body[4] != 0xff || body[5] != 0xe1) {
            return -1;
        }
    }

    iIndex += 4;//0xff
    
    /*sps*/
    iIndex += 1;//0xe1
    iIndex += 1;//sps len start
    sps_len = (size_t)body[iIndex++] << 8;
    sps_len += body[iIndex++];
    memcpy(sps, &body[iIndex], sps_len);
    iIndex +=  sps_len;

    /*pps*/
    iIndex++;//0x01
    pps_len = body[iIndex++] << 8;
    pps_len += body[iIndex++];
    memcpy(pps, &body[iIndex], pps_len);
    iIndex +=  pps_len;
    extra_len = iIndex;

    return 0;
}

int GetVpsSpsPpsFromHevcDecInfo(HEVC_DEC_CONF_RECORD* hevc_dec_info,
                                uint8_t* vps, size_t& vps_len,
                                uint8_t* sps, size_t& sps_len,
                                uint8_t* pps, size_t& pps_len)
{
    vps_len = 0;
    sps_len = 0;
    pps_len = 0;

    for(size_t i = 0; i < hevc_dec_info->nalu_vec.size(); i++) {
        HEVC_NALUnit nalu_unit = hevc_dec_info->nalu_vec[i];
        if (nalu_unit.nal_unit_type == NAL_UNIT_VPS) {
            vps_len = nalu_unit.nal_data_vec[0].nalu_data.size();
            memcpy(vps, (uint8_t*)(&nalu_unit.nal_data_vec[0].nalu_data[0]), vps_len);
        }
        if (nalu_unit.nal_unit_type == NAL_UNIT_SPS) {
            sps_len = nalu_unit.nal_data_vec[0].nalu_data.size();
            memcpy(sps, (uint8_t*)(&nalu_unit.nal_data_vec[0].nalu_data[0]), sps_len);
        }
        if (nalu_unit.nal_unit_type == NAL_UNIT_PPS) {
            pps_len = nalu_unit.nal_data_vec[0].nalu_data.size();
            memcpy(pps, (uint8_t*)(&nalu_unit.nal_data_vec[0].nalu_data[0]), pps_len);
        }
    }

    if ((vps_len == 0) || (sps_len == 0) || (pps_len == 0)) {
        return -1;
    }
    return 0;
}

std::string HevcDecInfoDump(HEVC_DEC_CONF_RECORD* hevc_dec_info) {
    std::stringstream ss;

    ss << "{";
    ss << "\"configuration_version\":" << (int)hevc_dec_info->configuration_version << ",";
    ss << "\"general_profile_space\":" << (int)hevc_dec_info->general_profile_space << ",";
    ss << "\"general_tier_flag\":" << (int)hevc_dec_info->general_tier_flag << ",";
    ss << "\"general_profile_idc\":" << (int)hevc_dec_info->general_profile_idc << ",";
    ss << "\"general_profile_compatibility_flags\":" << (int)hevc_dec_info->general_profile_compatibility_flags << ",";
    ss << "\"general_constraint_indicator_flags\":" << (int)hevc_dec_info->general_constraint_indicator_flags << ",";
    ss << "\"general_level_idc\":" << (int)hevc_dec_info->general_level_idc << ",";
    ss << "\"min_spatial_segmentation_idc\":" << (int)hevc_dec_info->min_spatial_segmentation_idc << ",";
    ss << "\"parallelism_type\":" << (int)hevc_dec_info->parallelism_type << ",";
    ss << "\"chroma_format\":" << (int)hevc_dec_info->chroma_format << ",";
    ss << "\"bitdepth_lumaminus8\":" << (int)hevc_dec_info->bitdepth_lumaminus8 << ",";
    ss << "\"bitdepth_chromaminus8\":" << (int)hevc_dec_info->bitdepth_chromaminus8 << ",";
    ss << "\"avg_framerate\":" << (int)hevc_dec_info->avg_framerate << ",";
    ss << "\"constant_frameRate\":" << (int)hevc_dec_info->constant_frameRate << ",";
    ss << "\"num_temporallayers\":" << (int)hevc_dec_info->num_temporallayers << ",";
    ss << "\"temporalid_nested\":" << (int)hevc_dec_info->temporalid_nested << ",";
    ss << "\"lengthsize_minusone\":" << (int)hevc_dec_info->lengthsize_minusone << ",";

    int i = 0;
    ss << "\"nalus\":" << "[";
    for(const HEVC_NALUnit& hevc_unit : hevc_dec_info->nalu_vec) {
        ss << "{";
        ss << "\"array_completeness\":" << (int)hevc_unit.array_completeness << ",";
        ss << "\"nal_unit_type\":" << (int)(hevc_unit.nal_unit_type) << ",";
        ss << "\"num_nalus\":" << (int)hevc_unit.num_nalus << ",";
        ss << "\"nal_data_vec\":" << "[";
        int index = 0;
        for (const HEVC_NALU_DATA& nalu_data : hevc_unit.nal_data_vec) {
            uint8_t* data = (uint8_t*)&(nalu_data.nalu_data[0]);
            std::string hex_str = DataToString(data, nalu_data.nalu_data.size());
            ss << "{";
            ss << "\"" << index++ << "\":" << "\"" << hex_str << "\"";
            ss << "}";
            if (index < hevc_unit.nal_data_vec.size()) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";
        if ((++i) < hevc_dec_info->nalu_vec.size()) {
            ss << ",";
        }
    }
    ss << "]";
    ss << "}";

    return ss.str();
}

int GetHevcDecInfoFromExtradata(HEVC_DEC_CONF_RECORD* hevc_dec_info, 
                                const uint8_t *extra_data, size_t extra_len)
{
    const uint8_t* p = extra_data;
    const uint8_t* end = extra_data + extra_len;

    hevc_dec_info->configuration_version = *p;
    if (hevc_dec_info->configuration_version != 1) {
        std::cout << "Invalid HEVC configuration version: " << (int)hevc_dec_info->configuration_version << std::endl;
        return -1;
    }
    p++;

    //general_profile_space(2bits), general_tier_flag(1bit), general_profile_idc(5bits)
    hevc_dec_info->general_profile_space = (*p >> 6) & 0x03;
    hevc_dec_info->general_tier_flag = (*p >> 5) & 0x01;
    hevc_dec_info->general_profile_idc = *p & 0x1F;
    p++;

    //general_profile_compatibility_flags: 32bits
    hevc_dec_info->general_profile_compatibility_flags = ByteStream::Read4Bytes(p);
    p += 4;

    //general_constraint_indicator_flags: 48bits
    int64_t general_constraint_indicator_flags = ByteStream::Read4Bytes(p);
    p += 4;
    general_constraint_indicator_flags = (general_constraint_indicator_flags << 16) | (ByteStream::Read2Bytes(p));
    p += 2;
    hevc_dec_info->general_constraint_indicator_flags = general_constraint_indicator_flags;

    //general_level_idc: 8bits
    hevc_dec_info->general_level_idc = *p;
    p++;

    //min_spatial_segmentation_idc: xxxx 14bits
    hevc_dec_info->min_spatial_segmentation_idc = ByteStream::Read2Bytes(p) & 0x0fff;
    p += 2;

    //parallelismType: xxxx xx 2bits
    hevc_dec_info->parallelism_type = *p & 0x03;
    p++;

    //chromaFormat: xxxx xx 2bits
    hevc_dec_info->chroma_format = *p & 0x03;
    p++;

    //bitDepthLumaMinus8: xxxx x 3bits
    hevc_dec_info->bitdepth_lumaminus8 = *p & 0x07;
    p++;

    //bitDepthChromaMinus8: xxxx x 3bits
    hevc_dec_info->bitdepth_chromaminus8 = *p & 0x07;
    p++;

    //avgFrameRate: 16bits
    hevc_dec_info->avg_framerate = ByteStream::Read2Bytes(p);
    p += 2;

    //8bits: constantFrameRate(2bits), numTemporalLayers(3bits), 
    //       temporalIdNested(1bit), lengthSizeMinusOne(2bits)
    hevc_dec_info->constant_frameRate  = (*p >> 6) & 0x03;
    hevc_dec_info->num_temporallayers  = (*p >> 3) & 0x07;
    hevc_dec_info->temporalid_nested   = (*p >> 2) & 0x01;
    hevc_dec_info->lengthsize_minusone = *p & 0x03;
    p++;

    uint8_t arrays_num = *p;
    p++;

    //parse vps/pps/sps
    for (int index = 0; index < arrays_num; index++) {
        HEVC_NALUnit hevc_unit;

        if ((p + 5) > end) {
            std::cout << "Invalid HEVC extra data, not enough data for nal unit header." << std::endl;
            return -1;
        }
        hevc_unit.array_completeness = (*p >> 7) & 0x01;
        hevc_unit.nal_unit_type = (*p) & 0x3f;
        p++;
        hevc_unit.num_nalus = ByteStream::Read2Bytes(p);
        p += 2;

        for (int i = 0; i < hevc_unit.num_nalus; i++) {
            HEVC_NALU_DATA data_item;
            uint16_t nalUnitLength = ByteStream::Read2Bytes(p);
            p += 2;

            if ((p + nalUnitLength) > end) {
                std::cout << "Invalid HEVC extra data, not enough data for nal unit, nalu len:" << nalUnitLength << std::endl;
                return -1;
            }
            //copy vps/pps/sps data
            data_item.nalu_data.resize(nalUnitLength);
            memcpy((uint8_t*)(&data_item.nalu_data[0]), p, nalUnitLength);
            p += nalUnitLength;

            hevc_unit.nal_data_vec.push_back(data_item);
        }
        hevc_dec_info->nalu_vec.push_back(hevc_unit);
    }
    
    return 0;
}

std::string LHevcDecInfoDump(LHEVC_DEC_CONF_RECORD* hevc_dec_info) {
    std::stringstream ss;

    ss << "{";
    ss << "\"configuration_version\":" << (int)hevc_dec_info->configuration_version << ",";
    ss << "\"min_spatial_segmentation_idc\":" << (int)hevc_dec_info->min_spatial_segmentation_idc << ",";
    ss << "\"parallelismType\":" << (int)hevc_dec_info->parallelismType << ",";
    ss << "\"numTemporalLayers\":" << (int)hevc_dec_info->numTemporalLayers << ",";
    ss << "\"temporalIdNested\":" << (int)hevc_dec_info->temporalIdNested << ",";
    ss << "\"lengthSizeMinusOne\":" << (int)hevc_dec_info->lengthSizeMinusOne << ",";
    ss << "\"numOfArrays\":" << (int)hevc_dec_info->numOfArrays << ",";

    int i = 0;
    ss << "\"nalus\":" << "[";
    for(const HEVC_NALUnit& hevc_unit : hevc_dec_info->nalu_vec) {
        ss << "{";
        ss << "\"array_completeness\":" << (int)hevc_unit.array_completeness << ",";
        ss << "\"nal_unit_type\":" << (int)(hevc_unit.nal_unit_type) << ",";
        ss << "\"num_nalus\":" << (int)hevc_unit.num_nalus << ",";
        ss << "\"nal_data_vec\":" << "[";
        int index = 0;
        for (const HEVC_NALU_DATA& nalu_data : hevc_unit.nal_data_vec) {
            uint8_t* data = (uint8_t*)&(nalu_data.nalu_data[0]);
            std::string hex_str = DataToString(data, nalu_data.nalu_data.size());
            ss << "{";
            ss << "\"" << index++ << "\":" << "\"" << hex_str << "\"";
            ss << "}";
            if (index < hevc_unit.nal_data_vec.size()) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";
        if ((++i) < hevc_dec_info->nalu_vec.size()) {
            ss << ",";
        }
    }
    ss << "]";
    ss << "}";
    return ss.str();
}

int GetLHevcDecInfoFromExtradata(LHEVC_DEC_CONF_RECORD* hevc_dec_info, 
                                const uint8_t *extra_data, size_t extra_len) {
    const uint8_t* p = extra_data;
    const uint8_t* end = extra_data + extra_len;

    hevc_dec_info->configuration_version = *p;
    p++;

    hevc_dec_info->reserved1 = 0;
    hevc_dec_info->min_spatial_segmentation_idc = ((uint16_t)(*p & 0x0f)) << 8;
    p++;
    hevc_dec_info->min_spatial_segmentation_idc |= *p & 0xff;
    p++;

    hevc_dec_info->reserved2 = 0;
    hevc_dec_info->parallelismType = *p & 0x03;
    p++;

    hevc_dec_info->reserved3 = 0;
    hevc_dec_info->numTemporalLayers = (*p & 0x38) > 3;
    hevc_dec_info->temporalIdNested = (*p & 0x04) > 2;
    hevc_dec_info->lengthSizeMinusOne = *p & 0x03;
    p++;

    hevc_dec_info->numOfArrays = *p;
    p++;

    for (int index = 0; index < hevc_dec_info->numOfArrays; index++) {
        HEVC_NALUnit hevc_unit;

        if ((p + 5) > end) {
            return -1;
        }
        hevc_unit.array_completeness = (*p >> 7) & 0x01;
        hevc_unit.nal_unit_type = (*p) & 0x3f;
        p++;
        hevc_unit.num_nalus = ByteStream::Read2Bytes(p);
        p += 2;

        for (int i = 0; i < hevc_unit.num_nalus; i++) {
            HEVC_NALU_DATA data_item;
            uint16_t nalUnitLength = ByteStream::Read2Bytes(p);
            p += 2;

            if ((p + nalUnitLength) > end) {
                return -1;
            }
            //copy vps/pps/sps data
            data_item.nalu_data.resize(nalUnitLength);
            memcpy((uint8_t*)(&data_item.nalu_data[0]), p, nalUnitLength);
            p += nalUnitLength;

            hevc_unit.nal_data_vec.push_back(data_item);
        }
        hevc_dec_info->nalu_vec.push_back(hevc_unit);
    }
    return 0;
}

// 移除防竞争字节(0x000003 -> 0x0000)
int RemoveEmulationPreventionBytes(const uint8_t* input, int input_size, uint8_t* output) {
    int output_pos = 0;
    int i = 0;
    
    while (i < input_size) {
        if (i + 2 < input_size && 
            input[i] == 0x00 && input[i + 1] == 0x00 && input[i + 2] == 0x03) {
            // 发现防竞争字节模式，跳过0x03
            output[output_pos++] = input[i++];     // 0x00
            output[output_pos++] = input[i++];     // 0x00
            i++;  // 跳过0x03
        } else {
            output[output_pos++] = input[i++];
        }
    }
    
    return output_pos;
}

// 初始化比特流读取器
void InitBitReader(BitReader* reader, const uint8_t* data, int size) {
    reader->data = data;
    reader->size = size;
    reader->bit_pos = 0;
}

// 读取指定数量的比特
uint32_t ReadBits(BitReader* reader, int n) {
    if (reader->bit_pos + n > reader->size * 8) {
        return 0; // 超出范围
    }
    
    uint32_t result = 0;
    for (int i = 0; i < n; i++) {
        int byte_pos = reader->bit_pos / 8;
        int bit_pos = reader->bit_pos % 8;
        
        if (reader->data[byte_pos] & (0x80 >> bit_pos)) {
            result |= (1 << (n - 1 - i));
        }
        reader->bit_pos++;
    }
    
    return result;
}

// 读取无符号指数哥伦布编码(ue(v))
uint32_t ReadUe(BitReader* reader) {
    int leading_zeros = 0;
    
    // 计算前导零的数量
    while (reader->bit_pos < reader->size * 8 && ReadBits(reader, 1) == 0) {
        leading_zeros++;
        if (leading_zeros > 32) {
            return 0; // 错误处理
        }
    }
    
    // 如果没有前导零，返回0
    if (leading_zeros == 0) {
        return 0;
    }
    
    // 读取剩余的比特
    uint32_t value = ReadBits(reader, leading_zeros);
    return (1 << leading_zeros) - 1 + value;
}

// 主要的HEVC SPS解析函数
int ParseHevcSpsFinal(const uint8_t* nalu_data, int nalu_size, int* width, int* height, Logger* logger) {
    if (!nalu_data || nalu_size < 15 || !width || !height) {
        LogErrorf(logger, "Invalid parameters");
        return -1; // 参数错误
    }
    
    // 分配缓冲区用于移除防竞争字节
    uint8_t* clean_data = (uint8_t*)malloc(nalu_size);
    if (!clean_data) {
        LogErrorf(logger, "Failed to allocate memory");
        return -1;
    }
    
    // 移除防竞争字节
    int clean_size = RemoveEmulationPreventionBytes(nalu_data, nalu_size, clean_data);
    
    // 初始化比特流读取器
    BitReader reader;
    InitBitReader(&reader, clean_data, clean_size);
    
    // 跳过NAL头部 (2字节 = 16比特)
    ReadBits(&reader, 16);
    
    // 解析SPS
    // sps_video_parameter_set_id (4 bits)
    ReadBits(&reader, 4);
    
    // sps_max_sub_layers_minus1 (3 bits)
    uint32_t sps_max_sub_layers_minus1 = ReadBits(&reader, 3);
    
    // sps_temporal_id_nesting_flag (1 bit)
    ReadBits(&reader, 1);
    
    // 跳过profile_tier_level结构
    // general_profile_space (2 bits)
    ReadBits(&reader, 2);
    // general_tier_flag (1 bit)  
    ReadBits(&reader, 1);
    // general_profile_idc (5 bits)
    ReadBits(&reader, 5);
    
    // general_profile_compatibility_flag[32] (32 bits)
    ReadBits(&reader, 32);
    
    // general_progressive_source_flag等 (6 bits)
    ReadBits(&reader, 6);
    
    // 跳过42个保留比特
    ReadBits(&reader, 32);
    ReadBits(&reader, 10);
    
    // general_level_idc (8 bits)
    ReadBits(&reader, 8);
    
    // 跳过sub_layer相关信息（如果有的话）
    for (int i = 0; i < sps_max_sub_layers_minus1; i++) {
        uint32_t sub_layer_profile_present_flag = ReadBits(&reader, 1);
        uint32_t sub_layer_level_present_flag = ReadBits(&reader, 1);
        
        if (sub_layer_profile_present_flag) {
            // 跳过sub layer profile信息
            ReadBits(&reader, 32);
            ReadBits(&reader, 32);
            ReadBits(&reader, 24);
        }
        
        if (sub_layer_level_present_flag) {
            ReadBits(&reader, 8);
        }
    }
    
    // sps_seq_parameter_set_id
    ReadUe(&reader);
    
    // chroma_format_idc
    uint32_t chroma_format_idc = ReadUe(&reader);
    
    if (chroma_format_idc == 3) {
        // separate_colour_plane_flag
        ReadBits(&reader, 1);
    }
    
    // pic_width_in_luma_samples - 这是我们需要的宽度
    uint32_t pic_width_in_luma_samples = ReadUe(&reader);
    
    // pic_height_in_luma_samples - 这是我们需要的高度  
    uint32_t pic_height_in_luma_samples = ReadUe(&reader);
    
    // conformance_window_flag
    uint32_t conformance_window_flag = ReadBits(&reader, 1);
    
    uint32_t conf_win_left_offset = 0;
    uint32_t conf_win_right_offset = 0;
    uint32_t conf_win_top_offset = 0;
    uint32_t conf_win_bottom_offset = 0;
    
    if (conformance_window_flag) {
        conf_win_left_offset = ReadUe(&reader);
        conf_win_right_offset = ReadUe(&reader);
        conf_win_top_offset = ReadUe(&reader);
        conf_win_bottom_offset = ReadUe(&reader);
    }
    
    // 计算最终的宽度和高度（考虑conformance window）
    uint32_t SubWidthC = (chroma_format_idc == 1 || chroma_format_idc == 2) ? 2 : 1;
    uint32_t SubHeightC = (chroma_format_idc == 1) ? 2 : 1;
    
    *width = pic_width_in_luma_samples - SubWidthC * (conf_win_left_offset + conf_win_right_offset);
    *height = pic_height_in_luma_samples - SubHeightC * (conf_win_top_offset + conf_win_bottom_offset);
    
    free(clean_data);
    return 0; // 成功
}

}
