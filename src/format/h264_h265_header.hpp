#ifndef H264_HEADER_HPP
#define H264_HEADER_HPP
#include "byte_stream.hpp"
#include "data_buffer.hpp"
#include "logger.hpp"
#include <stdint.h>
#include <string>
#include <vector>
#include <memory>

namespace cpp_streamer
{

enum NaluType : uint8_t
{
    kSlice         = 1,
    kIdr           = 5,
    kSei           = 6,
    kSps           = 7,
    kPps           = 8,
    kAud           = 9,
    kEndOfSequence = 10,
    kEndOfStream   = 11,
    kFiller        = 12,
    kStapA         = 24,
    kFuA           = 28,

    kReserved22    = 22,
    kReserved23    = 23,
    kReserved24    = 24,
    kReserved25    = 25,
    kReserved26    = 26,
    kReserved27    = 27,
    kReserved28    = 28,
    kReserved29    = 29,
    kReserved30    = 30,
    kReserved31    = 31
}; 

enum HEVC_NALU_TYPE
{
        NAL_UNIT_CODED_SLICE_TRAIL_N = 0,
        NAL_UNIT_CODED_SLICE_TRAIL_R, //1
        NAL_UNIT_CODED_SLICE_TSA_N,   //2
        NAL_UNIT_CODED_SLICE_TLA,     //3
        NAL_UNIT_CODED_SLICE_STSA_N,  //4
        NAL_UNIT_CODED_SLICE_STSA_R,  //5
        NAL_UNIT_CODED_SLICE_RADL_N,  //6
        NAL_UNIT_CODED_SLICE_DLP,     //7
        NAL_UNIT_CODED_SLICE_RASL_N,  //8
        NAL_UNIT_CODED_SLICE_TFD,     //9
        NAL_UNIT_RESERVED_10,
        NAL_UNIT_RESERVED_11,
        NAL_UNIT_RESERVED_12,
        NAL_UNIT_RESERVED_13,
        NAL_UNIT_RESERVED_14,
        NAL_UNIT_RESERVED_15,
        NAL_UNIT_CODED_SLICE_BLA,      //16
        NAL_UNIT_CODED_SLICE_BLANT,    //17
        NAL_UNIT_CODED_SLICE_BLA_N_LP, //18
        NAL_UNIT_CODED_SLICE_IDR,      //19
        NAL_UNIT_CODED_SLICE_IDR_N_LP, //20
        NAL_UNIT_CODED_SLICE_CRA,      //21
        NAL_UNIT_RESERVED_22,
        NAL_UNIT_RESERVED_23,
        NAL_UNIT_RESERVED_24,
        NAL_UNIT_RESERVED_25,
        NAL_UNIT_RESERVED_26,
        NAL_UNIT_RESERVED_27,
        NAL_UNIT_RESERVED_28,
        NAL_UNIT_RESERVED_29,
        NAL_UNIT_RESERVED_30,
        NAL_UNIT_RESERVED_31,
        NAL_UNIT_VPS,                   // 32
        NAL_UNIT_SPS,                   // 33
        NAL_UNIT_PPS,                   // 34
        NAL_UNIT_ACCESS_UNIT_DELIMITER, // 35
        NAL_UNIT_EOS,                   // 36
        NAL_UNIT_EOB,                   // 37
        NAL_UNIT_FILLER_DATA,           // 38
        NAL_UNIT_SEI ,                  // 39 Prefix SEI
        NAL_UNIT_SEI_SUFFIX,            // 40 Suffix SEI
        NAL_UNIT_RESERVED_41,
        NAL_UNIT_RESERVED_42,
        NAL_UNIT_RESERVED_43,
        NAL_UNIT_RESERVED_44,
        NAL_UNIT_RESERVED_45,
        NAL_UNIT_RESERVED_46,
        NAL_UNIT_RESERVED_47,
        NAL_UNIT_UNSPECIFIED_48,
        NAL_UNIT_UNSPECIFIED_49,
        NAL_UNIT_UNSPECIFIED_50,
        NAL_UNIT_UNSPECIFIED_51,
        NAL_UNIT_UNSPECIFIED_52,
        NAL_UNIT_UNSPECIFIED_53,
        NAL_UNIT_UNSPECIFIED_54,
        NAL_UNIT_UNSPECIFIED_55,
        NAL_UNIT_UNSPECIFIED_56,
        NAL_UNIT_UNSPECIFIED_57,
        NAL_UNIT_UNSPECIFIED_58,
        NAL_UNIT_UNSPECIFIED_59,
        NAL_UNIT_UNSPECIFIED_60,
        NAL_UNIT_UNSPECIFIED_61,
        NAL_UNIT_UNSPECIFIED_62,
        NAL_UNIT_UNSPECIFIED_63,
        NAL_UNIT_INVALID,
};

#define GET_H264_NALU_TYPE(code) ((code) & 0x1f)

#define GET_HEVC_NALU_TYPE(code) (HEVC_NALU_TYPE)((code & 0x7E)>>1)

typedef struct Hevc_Header_S {
    uint8_t forbid;
    uint8_t nalu_type;
    uint8_t layer_id;
    uint8_t tid;
} Hevc_Header;

typedef struct HEVC_NALU_DATA_S {
    std::vector<uint8_t>  nalu_data;
} HEVC_NALU_DATA;

typedef struct HEVC_NALUnit_S {
    uint8_t  array_completeness;
    uint8_t  nal_unit_type;
    uint16_t num_nalus;
    std::vector<HEVC_NALU_DATA> nal_data_vec;
} HEVC_NALUnit;

typedef struct HEVC_DEC_CONF_RECORD_S {
    uint8_t  configuration_version;
    uint8_t  general_profile_space;
    uint8_t  general_tier_flag;
    uint8_t  general_profile_idc;
    uint32_t general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    uint8_t  general_level_idc;
    uint16_t min_spatial_segmentation_idc;
    uint8_t  parallelism_type;
    uint8_t  chroma_format;
    uint8_t  bitdepth_lumaminus8;
    uint8_t  bitdepth_chromaminus8;
    uint16_t avg_framerate;
    uint8_t  constant_frameRate;
    uint8_t  num_temporallayers;
    uint8_t  temporalid_nested;
    uint8_t  lengthsize_minusone;
    std::vector<HEVC_NALUnit> nalu_vec;
} HEVC_DEC_CONF_RECORD;

// 9.3 L-HEVC elementary stream structure in ISO IEC 14496-15-2022.pdf
typedef struct LHEVC_DEC_CONF_RECORD_S {
    uint8_t  configuration_version;
    uint16_t reserved1:4;
    uint16_t min_spatial_segmentation_idc:12;

    uint8_t reserved2:6;
    uint8_t parallelismType:2;

    uint8_t reserved3:2;
    uint8_t numTemporalLayers:3;
    uint8_t temporalIdNested: 1;
    uint8_t lengthSizeMinusOne:2;
    
    uint8_t numOfArrays;
    std::vector<HEVC_NALUnit> nalu_vec;
} LHEVC_DEC_CONF_RECORD;

static const uint8_t H264_START_CODE[4] = {0x00, 0x00, 0x00, 0x01};
static const uint8_t H265_START_CODE[4] = {0x00, 0x00, 0x00, 0x01};

inline bool H264_IS_KEYFRAME(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kIdr);
}

inline bool H264_IS_AUD(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kAud);
}

inline bool H264_IS_SEI(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kSei);
}

inline bool H264_IS_SEQ(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kSps) || ((nalu_type & 0x1f) == kPps);
}

inline bool H264_IS_SPS(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kSps);
}

inline bool H264_IS_PPS(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kPps);
}

inline bool H264_IS_RESERVE(uint8_t nalu_type) {
    nalu_type = (nalu_type & 0x1f);

    return (nalu_type >= kReserved22) && (nalu_type <= kReserved31);
}

inline int GetNaluTypePos(uint8_t* data) {
    if ((data[0] == 0) && (data[1] == 0) && (data[2] == 0x01)) {
        return 3;
    }

    if ((data[0] == 0) && (data[1] == 0) && (data[2] == 0) && (data[3] == 0x01)) {
        return 4;
    }
    return -1;
}

inline bool Is_AnnexB_Header(uint8_t* data, size_t len) {
    if (len < 4) {
        return false;
    }
    if (data[0] == 0x00 && data[1] == 0x00 &&
        data[2] == 0x00 && data[3] == 0x01) {
        return true;
    }
    if (data[0] == 0x00 && data[1] == 0x00 &&
        data[2] == 0x01) {
        return true;
    }
    return false;
}


bool AnnexB2Nalus(uint8_t* data, size_t len, std::vector<std::shared_ptr<DataBuffer>>& nalus);

bool AnnexB2Avcc(uint8_t* data, size_t len, std::vector<std::shared_ptr<DataBuffer>>& nalus);

bool Avcc2Nalus(uint8_t* data, size_t len, std::vector<std::shared_ptr<DataBuffer>>& nalus);

int GetSpsPpsFromExtraData(uint8_t *pps, size_t& pps_len, 
                           uint8_t *sps, size_t& sps_len, 
                           const uint8_t *extra_data, size_t extra_len);

int GetVpsSpsPpsFromHevcDecInfo(HEVC_DEC_CONF_RECORD* hevc_dec_info,
                                uint8_t* vps, size_t& vps_len,
                                uint8_t* sps, size_t& sps_len,
                                uint8_t* pps, size_t& pps_len);


int GetHevcDecInfoFromExtradata(HEVC_DEC_CONF_RECORD* hevc_dec_info, 
                                const uint8_t *extra_data, size_t extra_len);

std::string HevcDecInfoDemp(HEVC_DEC_CONF_RECORD* hevc_dec_info);

void GetHevcHeader(uint8_t* data, Hevc_Header& header);
std::string HevcHeaderDump(const Hevc_Header& header);

int GetLHevcDecInfoFromExtradata(LHEVC_DEC_CONF_RECORD* hevc_dec_info, 
                                const uint8_t *extra_data, size_t extra_len);
std::string LHevcDecInfoDump(LHEVC_DEC_CONF_RECORD* hevc_dec_info);

}
#endif

