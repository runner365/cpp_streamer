#ifndef MP4_BOX_HPP
#define MP4_BOX_HPP
#include "byte_stream.hpp"
#include "stringex.hpp"
#include "av.hpp"
#include "h264_h265_header.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <assert.h>
#include <sstream>
#include <iostream>

namespace cpp_streamer
{

class MvhdBox;
class FtypBox;
class MoovBox;
class TkhdBox;
class EdtsBox;
class MdiaBox;
class ElstBox;
class MdhdBox;
class HdlrBox;
class MinfBox;
class VmhdBox;
class DinfBox;
class StblBox;
class StsdBox;
class SttsBox;
class StscBox;
class StszBox;
class StcoBox;
class DrefBox;
class TrakBox;
class UdtaBox;

inline uint64_t GetBoxHeaderInfo(uint8_t* p, std::string& box_type, int& offset) {
    uint64_t box_size = ByteStream::Read4Bytes(p);

    if (box_size == 1) {
        box_size = ByteStream::Read8Bytes(p + 8);
        offset = 8 + 8;
    } else {
        offset = 8;
    }

    box_type = DataToString((char*)p + 4, 4);
    
    return box_size;
}

typedef struct SampleEntry_S
{
    uint32_t sample_count_;
    uint32_t samples_delta_;
} SampleEntry;

typedef struct SampleOffset_S {
    uint32_t sample_counts_;
    uint32_t sample_offsets_;//cts = pts - dts;
} SampleOffset;

typedef struct ChunkSample_S {
    uint32_t first_chunk_;
    uint32_t samples_per_chunk_;
    uint32_t sample_description_index_;
} ChunkSample;

typedef struct MovItem_S {
    MEDIA_PKT_TYPE av_type_;
    MEDIA_CODEC_TYPE codec_type_;
    size_t offset;
    size_t len;
    int64_t dts;//microsecond
    int64_t pts;//microsecond
    int64_t timescale_;//1000000
    bool is_keyframe_;
} MovItem;

class TrakInfo
{
public:
    TrakInfo()
    {
    }
    ~TrakInfo()
    {
    }

public:
    uint32_t track_id_;
    uint32_t timescale_;
    double duration_;//microsecond
    std::string handler_type_;// "soun", "vide"
    MEDIA_CODEC_TYPE codec_type_ = MEDIA_CODEC_UNKOWN;

    uint32_t width_;
    uint32_t height_;
    uint32_t horizontal_resolution_;
    uint32_t vertical_resolution_;

    uint16_t channelcount_;
    uint16_t samplesize_;
    uint32_t samplerate_;
    uint32_t buffer_size_;
    uint32_t max_bit_rate_;
    uint32_t avg_bit_rate_;

    std::vector<uint8_t> sequence_data_;
    LHEVC_DEC_CONF_RECORD lhevc_dec_conf_;

    std::vector<SampleEntry> sample_entries_;//stts: sample, duration for dts
    std::vector<SampleOffset> sample_offset_vec_;//ctts: sample cts list(cts = pts - dts) to get pts
    std::vector<uint32_t> iframe_sample_vec_;//stss: sample I frame position
    std::vector<ChunkSample> chunk_sample_vec_;//stsc: {first_chunk, sample_per_chunk, desc_index}
    std::vector<uint32_t> sample_sizes_vec_;//stsz: each sample size
    std::vector<uint32_t> chunk_offsets_vec_;//stco: each chunk offset
};

class MovInfo
{
public:
    MovInfo()
    {
    }
    ~MovInfo()
    {
    }

public:
    std::string major_brand_;
    uint32_t minor_version_;
    std::vector<std::string> compatible_brands_;

    double duration_ = 0;//microsecond
    uint32_t next_track_id_ = 0;

public:
    std::vector<TrakInfo> traks_info_;
};

class Mp4BoxBase
{
public:
    Mp4BoxBase() {}
    ~Mp4BoxBase() {}

public:
    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = start;
        box_size_ = ByteStream::Read4Bytes(p);
        type_ = DataToString((char*)(p + 4), 4);

        if (box_size_ == 1) {
            box_size_ = ByteStream::Read8Bytes(p + 8);
            offset_ = 8 + 8;
            return start + 8 + 8;
        }
        
        offset_ = 8;
        return start + 8;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << "";
        ss << "}";

        return ss.str();
    }
public:
    uint64_t box_size_ = 0;
    std::string type_;
    uint8_t* start_ = nullptr;
    int offset_ = 0;

    Mp4BoxBase* father_ = nullptr;
    std::vector<Mp4BoxBase*> childrens_;
};

#define BRANDS_MAX 8

//styp box is a root box for fmp4
class StypBox : public Mp4BoxBase
{
public:
    StypBox() { type_ = "styp"; }
    ~StypBox() {}

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        major_brand_ = DataToString((char*)p, 4);
        minor_version_ = ByteStream::Read4Bytes(p + 4);

        compatible_brands_.clear();
        int brands_count = ((int)box_size_ - offset_ - 8) / 4;
        for (int i = 0; i < brands_count; ++i) {
            compatible_brands_.push_back(DataToString((char*)p + 8 + i * 4, 4));
        }
        return p + 8 + brands_count * 4;
    }

    uint8_t* Serialize(size_t& len) {
        len = 8 + 8 + 4 * compatible_brands_.size();
        uint8_t* buf = new uint8_t[len];
        // box size
        buf[0] = (len >> 24) & 0xFF;
        buf[1] = (len >> 16) & 0xFF;
        buf[2] = (len >> 8) & 0xFF;
        buf[3] = len & 0xFF;
        // box type "styp"
        buf[4] = 's'; buf[5] = 't'; buf[6] = 'y'; buf[7] = 'p';
        // major_brand
        memcpy(buf + 8, major_brand_.c_str(), 4);
        // minor_version
        buf[12] = (minor_version_ >> 24) & 0xFF;
        buf[13] = (minor_version_ >> 16) & 0xFF;
        buf[14] = (minor_version_ >> 8) & 0xFF;
        buf[15] = minor_version_ & 0xFF;
        // compatible_brands
        for (size_t i = 0; i < compatible_brands_.size(); ++i) {
            memcpy(buf + 16 + i * 4, compatible_brands_[i].c_str(), 4);
        }
        return buf;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"major_brand\":\"" << major_brand_ << "\",";
        ss << "\"minor_version\":" << minor_version_ << ",";
        ss << "\"compatible_brands\":[";
        for (size_t i = 0; i < compatible_brands_.size(); ++i) {
            ss << "\"" << compatible_brands_[i] << "\"";
            if (i != compatible_brands_.size() - 1) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";
        return ss.str();
    }
public:
    std::string major_brand_;
    uint32_t minor_version_ = 0;
    std::vector<std::string> compatible_brands_ = {"msdh", "msix"};
};

//ftyp is a root box
class FtypBox : public Mp4BoxBase
{
public:
    FtypBox() { type_ = "ftyp"; }
    ~FtypBox() {}

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        major_brand_ = ByteStream::Read4Bytes(p);
        mov.major_brand_ = Uint32ToString(major_brand_);
        p += 4;
        minor_version_ = ByteStream::Read4Bytes(p);
        mov.minor_version_ = minor_version_;
        p += 4;

        int i = 0;
        while (p < (start + box_size_)) {
            brands_count_++;
            compatible_brands_[i++] = ByteStream::Read4Bytes(p);
            mov.compatible_brands_.push_back(Uint32ToString(compatible_brands_[i-1]));
            p += 4;
        }
        assert(p == (start + box_size_));
        return start + box_size_;
    }
    uint8_t* Serialize(size_t& len) {
        serialize_data_.resize(80);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve box size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG('f', 't', 'y', 'p'));//ftyp
        p += 4;
        ByteStream::Write4Bytes(p, major_brand_);//major brand
        p += 4;
        ByteStream::Write4Bytes(p, minor_version_);//minor version
        p += 4;
        for (size_t i = 0; i < brands_count_; i++) {
            ByteStream::Write4Bytes(p, compatible_brands_[i]);//compatible brand
            p += 4;
        }
        len = p - data;
        box_size_ = len;

        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"major_brand\":" << Uint32ToString(major_brand_) << ",";
        ss << "\"minor_version\":" << minor_version_ << ",";
        ss << "\"brands_count:\":" << brands_count_ << ",";
        ss << "\"compatible_brands\":[";
        for (size_t i = 0; i < brands_count_; i++) {
            ss << Uint32ToString(compatible_brands_[i]);
            if (i != (brands_count_ - 1)) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";
        return ss.str();
    }
public:
    uint32_t major_brand_; //eg. isom
    uint32_t minor_version_; //eg. 512
    uint32_t compatible_brands_[BRANDS_MAX]; //eg. isom,iso2,avc1,mp41
    size_t brands_count_;

private:
    std::vector<uint8_t> serialize_data_;
};

//mvhd is in moov
class MvhdBox : public Mp4BoxBase
{
public:
    MvhdBox() { type_ = "mvhd"; }
    ~MvhdBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 512;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];

        memset(data, 0, default_size);

        uint8_t* p = data;
        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, creation_time_);
        p += 4;
        ByteStream::Write4Bytes(p, modification_time_);
        p += 4;
        ByteStream::Write4Bytes(p, timescale_);
        p += 4;
        ByteStream::Write4Bytes(p, duration_);
        p += 4;
        ByteStream::Write4Bytes(p, rate_);
        p += 4;
        ByteStream::Write2Bytes(p, volume_);
        p += 2;
        ByteStream::Write2Bytes(p, 0);// reserve1_
        p += 2;
        ByteStream::Write4Bytes(p, 0);// reserve2_[0]
        p += 4;
        ByteStream::Write4Bytes(p, 0);// reserve2_[1]
        p += 4;

        for (size_t i = 0; i < sizeof(matrix_)/sizeof(uint32_t); i++) {
            ByteStream::Write4Bytes(p, matrix_[i]);
            p += 4;
        }
        for (size_t i = 0; i < sizeof(pre_defined_)/sizeof(uint32_t); i++) {
            ByteStream::Write4Bytes(p, pre_defined_[i]);
            p += 4;
        }
        ByteStream::Write4Bytes(p, next_track_id_);
        p += 4;

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        creation_time_ = ByteStream::Read4Bytes(p);
        p += 4;
        modification_time_ = ByteStream::Read4Bytes(p);
        p += 4;
        timescale_ = ByteStream::Read4Bytes(p);
        p += 4;
        duration_ = ByteStream::Read4Bytes(p);
        if (timescale_ != 0) {
            mov.duration_ = duration_ * 1000000.0 / timescale_;
        } else {
            mov.duration_ = duration_ * 1.0;
        }
        
        p += 4;
        rate_ = ByteStream::Read4Bytes(p);
        p += 4;
        volume_ = ByteStream::Read2Bytes(p);
        p += 2;
        reserve1_ = ByteStream::Read2Bytes(p);
        p += 2;
        reserve2_[0] = ByteStream::Read4Bytes(p);
        p += 4;
        reserve2_[1] = ByteStream::Read4Bytes(p);
        p += 4;

        for (size_t i = 0; i < sizeof(matrix_)/sizeof(uint32_t); i++) {
            matrix_[i] = ByteStream::Read4Bytes(p);
            p += 4;
        }
        for (size_t i = 0; i < sizeof(pre_defined_)/sizeof(uint32_t); i++) {
            pre_defined_[i] = ByteStream::Read4Bytes(p);
            p += 4;
        }
        next_track_id_ = ByteStream::Read4Bytes(p);
        mov.next_track_id_ = next_track_id_;
        p += 4;

        assert(p == (start + box_size_));
        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"creation_time\":" << creation_time_ << ",";
        ss << "\"modification_time\":" << modification_time_ << ",";
        ss << "\"timescale\":" << timescale_ << ",";
        ss << "\"duration\":" << duration_ << ",";
        ss << "\"rate\":" << rate_ << ",";
        ss << "\"volume\":" << volume_ << ",";
        ss << "\"reserve1\":" << reserve1_ << ",";
        ss << "\"reserve2[0]\":" << reserve2_[0] << ",";
        ss << "\"reserve2[1]\":" << reserve2_[1] << ",";
        ss << "\"matrix\":[";
        for (size_t i = 0; i < sizeof(matrix_)/sizeof(uint32_t); i++) {
            ss << matrix_[i] << (i != sizeof(matrix_)/sizeof(uint32_t) - 1 ? "," : "");
        }
        ss << "],";
        ss << "\"pre_defined\":[";
        for (size_t i = 0; i < sizeof(pre_defined_)/sizeof(uint32_t); i++) {
            ss << pre_defined_[i] << (i != sizeof(pre_defined_)/sizeof(uint32_t) - 1 ? "," : "");
        }
        ss << "],";
        ss << "\"next_track_id\":" << next_track_id_;
        ss << "}";

        return ss.str();
    }
public:
    uint32_t version_flag_ = 0; //version:8, 0 or 1; flags: 0
    uint32_t creation_time_ = 0;
    uint32_t modification_time_ = 0;
    uint32_t timescale_ = 0;
    uint32_t duration_ = 0;
    uint32_t rate_ = 0x00010000;
    uint16_t volume_ = 0x0100;
    uint16_t reserve1_ = 0;
    uint32_t reserve2_[2] = { 0 };
    uint32_t matrix_[9] = { 65536,0,0,0,65536,0,0,0,1073741824 }; //65536,0,0,0,65536,0,0,0,1073741824
    uint32_t pre_defined_[6] = { 0 };
    uint32_t next_track_id_;

private:
    std::vector<uint8_t> serialize_data_;
};

//tkhd is in trak
class TkhdBox : public Mp4BoxBase
{
public:
    TkhdBox() { type_ = "tkhd"; }
    ~TkhdBox() {}

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_flag_ = ByteStream::Read4Bytes(p);
        uint8_t ver = (uint8_t)(version_flag_ >> 24);
        p += 4;

        creation_time_ = (ver == 0) ? ByteStream::Read4Bytes(p) : ByteStream::Read8Bytes(p);
        p += (ver == 0) ? 4 : 8;

        modification_time_ = (ver == 0) ? ByteStream::Read4Bytes(p) : ByteStream::Read8Bytes(p);
        p += (ver == 0) ? 4 : 8;

        track_id_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].track_id_ = track_id_;
        p += 4;

        reserved1_ = ByteStream::Read4Bytes(p);
        p += 4;

        duration_ = (ver == 0) ? ByteStream::Read4Bytes(p) : ByteStream::Read8Bytes(p);
        mov.traks_info_[index].duration_ = duration_ * 1.0;
        p += (ver == 0) ? 4 : 8;

        reserved2_[0] = ByteStream::Read4Bytes(p);
        p += 4;

        reserved2_[1] = ByteStream::Read4Bytes(p);
        p += 4;

        layer_ = ByteStream::Read2Bytes(p);
        p += 2;

        alternate_group_ = ByteStream::Read2Bytes(p);
        p += 2;

        volume_ = ByteStream::Read2Bytes(p);
        p += 2;

        reserved3_ = ByteStream::Read2Bytes(p);
        p += 2;


        for(size_t i = 0; i < sizeof(transform_matrix_)/sizeof(uint32_t); i++) {
            transform_matrix_[i] = ByteStream::Read4Bytes(p);
            p += 4;
        }

        width_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].width_ = width_ >> 16;
        p += 4;

        height_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].height_ = height_ >> 16;
        p += 4;

        assert(p == start + box_size_);
        return start + box_size_;
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_len = 1024;
        serialize_data_.resize(default_len);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        memset(data, 0, default_len);

        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;

        uint8_t ver = (uint8_t)(version_flag_ >> 24);
        if (ver != 0) {
            ByteStream::Write8Bytes(p, creation_time_);
            p += 8;
            ByteStream::Write8Bytes(p, modification_time_);
            p += 8;
            ByteStream::Write4Bytes(p, track_id_);
            p += 4;
            ByteStream::Write4Bytes(p, 0);//reserved
            p += 4;
            ByteStream::Write8Bytes(p, duration_);
            p += 8;
        } else {
            ByteStream::Write4Bytes(p, (uint32_t)creation_time_);
            p += 4;
            ByteStream::Write4Bytes(p, (uint32_t)modification_time_);
            p += 4;
            ByteStream::Write4Bytes(p, track_id_);
            p += 4;
            ByteStream::Write4Bytes(p, 0);//reserved
            p += 4;
            ByteStream::Write4Bytes(p, (uint32_t)duration_);
            p += 4;
        }
        ByteStream::Write4Bytes(p, 0);//reserved
        p += 4;
        ByteStream::Write4Bytes(p, 0);//reserved
        p += 4;
        ByteStream::Write2Bytes(p, layer_);
        p += 2;
        ByteStream::Write2Bytes(p, alternate_group_);
        p += 2;
        ByteStream::Write2Bytes(p, volume_);
        p += 2;
        ByteStream::Write2Bytes(p, 0);//reserved
        p += 2;

        for(size_t i = 0; i < sizeof(transform_matrix_)/sizeof(uint32_t); i++) {
            ByteStream::Write4Bytes(p, transform_matrix_[i]);
            p += 4;
        }

        ByteStream::Write4Bytes(p, width_);
        p += 4;
        ByteStream::Write4Bytes(p, height_);
        p += 4;

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"creation_time\":" << creation_time_ << ",";
        ss << "\"modification_time\":" << modification_time_ << ",";
        ss << "\"track_id\":" << track_id_ << ",";
        ss << "\"reserved1\":" << reserved1_ << ",";
        ss << "\"duration\":" << duration_ << ",";
        ss << "\"reserved2[0]\":" << reserved2_[0] << ",";
        ss << "\"reserved2[1]\":" << reserved2_[1] << ",";
        ss << "\"layer\":" << layer_ << ",";
        ss << "\"alternate_group\":" << alternate_group_ << ",";
        ss << "\"volume\":" << volume_ << ",";
        ss << "\"reserved3\":" << reserved3_ << ",";
        
        ss << "\"transform_matrix\":[";
        for(size_t i = 0; i < sizeof(transform_matrix_)/sizeof(uint32_t); i++) { 
            ss << transform_matrix_[i];
            if (i != (sizeof(transform_matrix_)/sizeof(uint32_t) - 1)) {
                ss << ",";
            }
        }
        ss << "],";
        ss << "\"width\":" << width_ << ",";
        ss << "\"height\":" << height_;
        ss << "}";

        return ss.str();
    }

public:
    /*version: 8bits, 0 or 1, 
      flags: 24bits
      Bit 0: this bit is set if the track is 
      enabled
      Bit 1 = this bit is set if the track is part of 
      the presentation
      Bit 2 = this bit is set if the track should 
      be considered when previewing the file*/
    uint32_t version_flag_ = 0;
    uint64_t creation_time_ = 0; //if version == 0, it's 32bits; if version == 1, it's 64bits
    uint64_t modification_time_ = 0; //if version == 0, it's 32bits; if version == 1, it's 64bits
    uint32_t track_id_ = 0; //track_id 32bits
    uint32_t reserved1_ = 0;
    uint64_t duration_ = 0; //if version == 0, it's 32bits; if version == 1, it's 64bits
    uint32_t reserved2_[2];

    uint16_t layer_ = 0;
    uint16_t alternate_group_ = 0;

    uint16_t volume_ = 0;
    uint16_t reserved3_ = 0;

    uint32_t transform_matrix_[9] = {65536, 0, 0, 0, 65536, 0, 0, 0, 1073741824};
    uint32_t width_ = 0;
    uint32_t height_ = 0;

private:
    std::vector<uint8_t> serialize_data_;
};

//mdhd is in mdia
class MdhdBox : public Mp4BoxBase
{
public:
    MdhdBox() { type_ = "mdhd"; }
    ~MdhdBox() {}

public:
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;
        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;

        uint8_t ver = (uint8_t)(version_flag_ >> 24);
        if (ver == 0) {
            ByteStream::Write4Bytes(p, (uint32_t)creation_time_); //placeholder for creation_time
            p += 4;
            ByteStream::Write4Bytes(p, (uint32_t)modification_time_); //placeholder for modification_time
            p += 4;
            ByteStream::Write4Bytes(p, (uint32_t)timescale_); //placeholder for timescale
            p += 4;
            ByteStream::Write4Bytes(p, (uint32_t)duration_); //placeholder for duration
            p += 4;
        } else {
            ByteStream::Write8Bytes(p, (uint64_t)creation_time_); //placeholder for creation_time
            p += 8;
            ByteStream::Write8Bytes(p, (uint64_t)modification_time_); //placeholder for modification_time
            p += 8;
            ByteStream::Write4Bytes(p, (uint32_t)timescale_); //placeholder for timescale
            p += 4;
            ByteStream::Write8Bytes(p, (uint64_t)duration_); //placeholder for duration
            p += 8;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;
        
        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        uint8_t ver = (uint8_t)(version_flag_ >> 24);
        if (ver == 0) {
            creation_time_ = ByteStream::Read4Bytes(p);
            p += 4;
            modification_time_ = ByteStream::Read4Bytes(p);
            p += 4;
            timescale_ = ByteStream::Read4Bytes(p);
            p += 4;
            duration_ = ByteStream::Read4Bytes(p);
            p += 4;
        } else {
            creation_time_ = ByteStream::Read8Bytes(p);
            p += 8;
            modification_time_ = ByteStream::Read8Bytes(p);
            p += 8;
            timescale_ = ByteStream::Read4Bytes(p);
            p += 4;
            duration_ = ByteStream::Read8Bytes(p);
            p += 8;
        }
        mov.traks_info_[index].duration_ = duration_ * 1000000.0 / timescale_;
        mov.traks_info_[index].timescale_ = timescale_;
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"creation_time\":" << creation_time_ << ",";
        ss << "\"modification_time\":" << modification_time_ << ",";
        ss << "\"timescale\":" << timescale_ << ",";
        ss << "\"duration\":" << duration_ << ",";
        ss << "\"language\":" << language_ << ",";
        ss << "\"quality\":" << quality_;
        ss << "}";
        return ss.str();
    }

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint64_t creation_time_ = 0; //if version == 0, it's 32bits; if version == 1, it's 64bits
    uint64_t modification_time_ = 0; //if version == 0, it's 32bits; if version == 1, it's 64bits
    uint32_t timescale_ = 0;
    uint64_t duration_ = 0;//if version == 0, it's 32bits; if version == 1, it's 64bits
    uint16_t language_ = 0;
    uint16_t quality_  = 0;
private:
    std::vector<uint8_t> serialize_data_;
};

//hdlr is in mdia
class HdlrBox : public Mp4BoxBase
{
public:
    HdlrBox() { type_ = "hdlr"; }
    ~HdlrBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, pre_defined_);
        p += 4;
        ByteStream::Write4Bytes(p, handler_type_);
        p += 4;
        for (size_t i = 0; i < sizeof(reserved_)/sizeof(uint32_t); i++) {
            ByteStream::Write4Bytes(p, reserved_[i]);
            p += 4;
        }
        // std::cout << "hdlr box serialize name:" << handler_descr_ << std::endl;
        memcpy(p, handler_descr_.data(), handler_descr_.length());
        p += handler_descr_.length();
        *p = 0;
        p++;

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        pre_defined_ = ByteStream::Read4Bytes(p);
        p += 4;
        handler_type_ = ByteStream::Read4Bytes(p);
        p += 4;
        for (size_t i = 0; i < sizeof(reserved_)/sizeof(uint32_t); i++) {
            reserved_[i] = ByteStream::Read4Bytes(p);
            p += 4;
        }
        handler_descr_ = std::string((char*)p, start + box_size_ - p);
        // std::cout << "hdlr box parse dscr:" << handler_descr_ << std::endl;

        std::string type_str = Uint32ToString(handler_type_);
        mov.traks_info_[index].handler_type_ = type_str;
        // std::cout << "hdlr box parse type:" << type_str << std::endl;

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"pre_defined\":" << pre_defined_ << ",";
        ss << "\"handler_type\":\"" << Uint32ToString(handler_type_) << "\",";
        ss << "\"reserved\":[";
        for (size_t i = 0; i < sizeof(reserved_)/sizeof(uint32_t); i++) {
            ss << reserved_[i];
            if (i != (sizeof(reserved_)/sizeof(uint32_t) - 1)) {
                ss << ",";
            }
        }
        ss << "],";
        ss << "\"name\":\"" << handler_descr_ << "\"";
        ss << "}";
        return ss.str();
    }

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t pre_defined_ = 0;
    uint32_t handler_type_ = 0;//"vide" or "soun"
    uint32_t reserved_[3];
    std::string handler_descr_;

private:
    std::vector<uint8_t> serialize_data_;
};

//smhd is in minf
class SmhdBox : public Mp4BoxBase
{
public:
    SmhdBox() { type_ = "smhd"; }
    ~SmhdBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write2Bytes(p, balance_);
        p += 2;
        ByteStream::Write2Bytes(p, reserved_);
        p += 2;

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        balance_ = ByteStream::Read2Bytes(p);
        p += 2;
        reserved_ = ByteStream::Read2Bytes(p);
        p += 2;
        assert(p == (start + box_size_));

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"balance\":" << balance_ << ",";
        ss << "\"reserved\":" << reserved_;
        ss << "}";

        return ss.str();
    }

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint16_t balance_  = 0;
    uint16_t reserved_ = 0;

private:
    std::vector<uint8_t> serialize_data_;
};

class VmhdBox : public Mp4BoxBase
{
public:
    VmhdBox() { type_ = "vmhd"; }
    ~VmhdBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write2Bytes(p, graphicsmode_);
        p += 2;
        for (size_t i = 0; i < sizeof(opcolor_); i++) {
            ByteStream::Write2Bytes(p, opcolor_[i]);
            p += 2;
        }
        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;

        graphicsmode_ = ByteStream::Read2Bytes(p);
        p += 2;
        for (size_t i = 0; i < sizeof(opcolor_); i++) {
            opcolor_[i] = ByteStream::Read2Bytes(p);
            p += 2;
        }
        assert(p == (start + box_size_));
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"graphicsmode\":" << (int)graphicsmode_ << ",";
        ss << "\"opcolor\":[";
        for (size_t i = 0; i < sizeof(opcolor_); i++) {
            ss << (int)opcolor_[i];
            if (i != (sizeof(opcolor_) - 1)) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";

        return ss.str();
    }
public:
    uint32_t version_flag_ = 0x01;//version: 8 bits, flag:24bits
    uint8_t graphicsmode_ = 0;
    uint8_t opcolor_[3] = { 0 };

private:
    std::vector<uint8_t> serialize_data_;
};

//url is in DrefBox
class UrlBox : public Mp4BoxBase
{
public:
    UrlBox() { type_ = "url "; }
    ~UrlBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        memcpy(p, location_.c_str(), location_.length());
        p += location_.length();

        // std::cout << "url box dump:" << location_ <<", len:" << location_.length() << std::endl;
        // std::cout << "box size:" << p - data << std::endl;
        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;

        size_t str_len = start + box_size_ - p;
        location_ = std::string((char*)p, str_len);
        
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"location\":\"" << location_ << "\"";
        ss << "}";
        return ss.str();
    }

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    std::string location_ = "default url";

private:
    std::vector<uint8_t> serialize_data_;
};

//dref is in dinf
class DrefBox : public Mp4BoxBase
{
public:
    DrefBox() { type_ = "dref"; }
    ~DrefBox() {
        for (UrlBox* box : urls_box_) {
            delete box;
        }
        urls_box_.clear();
    }
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;

        ByteStream::Write4Bytes(p, (uint32_t)urls_box_.size());
        p += 4;

        for (UrlBox* box : urls_box_) {
            uint8_t* box_data = box->Serialize(len);
            memcpy(p, box_data, len);
            p += len;
        }
        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;

        entry_count_ = ByteStream::Read4Bytes(p);
        p += 4;
        for (size_t i = 0; i < entry_count_; i++) {
            UrlBox* url_box = new UrlBox();
            p = url_box->Parse(p);
            urls_box_.push_back(url_box);
        }
        assert(p == (start + box_size_));
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"entry_count\":" << entry_count_ << ",";
        ss << "\"urlboxs\":[";
        for (size_t i = 0; i < urls_box_.size(); i++) {
            ss << urls_box_[i]->Dump();
            if (i != (urls_box_.size() - 1)) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";
        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<UrlBox*> urls_box_;

private:
    std::vector<uint8_t> serialize_data_;
};

//dinf is in minf, it has dref
class DinfBox : public Mp4BoxBase
{
public:
    DinfBox() { type_ = "dinf"; }
    ~DinfBox() {
        if (dref_) {
            delete dref_;
            dref_ = nullptr;
        }
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (dref_) {
            uint8_t* dref_data = dref_->Serialize(len);
            memcpy(p, dref_data, len);
            p += len;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        dref_ = new DrefBox();
        p = dref_->Parse(p);

        assert(p == (start + box_size_));
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"dref\":" << dref_->Dump();
        ss << "}";
        return ss.str();
    }
public:
    DrefBox* dref_ = nullptr;

private:
    std::vector<uint8_t> serialize_data_;
};

static MEDIA_CODEC_TYPE GetCodecTypeByBoxType(const std::string& box_type) {
    // std::cout << "GetCodecTypeByBoxType, box_type:" << box_type << std::endl;

    if (box_type == "avcC") {
        return MEDIA_CODEC_H264;
    } else if (box_type == "hvcC" || box_type == "hvc1") {
        return MEDIA_CODEC_H265;
    } else if (box_type == "vvcC") {
        return MEDIA_CODEC_H266;
    } else if (box_type == "av1C") {
        return MEDIA_CODEC_AV1;
    } else if (box_type == "vp09") {
        return MEDIA_CODEC_VP9;
    }

    if (box_type == "mp4a") {
        return MEDIA_CODEC_AAC;
    } else if (box_type == "Opus") {
        return MEDIA_CODEC_OPUS;
    }
    return MEDIA_CODEC_UNKOWN;
}

//next box: avcC(h264), hvcC(h265), av1C(av1), vvcC(h266), vpcC(vp8, vp9)
class VideoSequenceBox : public Mp4BoxBase
{
public:
    VideoSequenceBox() {
        type_ = "avcC";
    }
    ~VideoSequenceBox() {}
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (data_.size() > 0) {
            memcpy(p, &(data_[0]), data_.size());
            p += data_.size();
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        size_t data_len = box_size_ - (p - start);

        mov.traks_info_[index].codec_type_ = GetCodecTypeByBoxType(type_);
        data_.resize(data_len);
        mov.traks_info_[index].sequence_data_.resize(data_len);
        uint8_t* header = (uint8_t*)&(data_[0]);

        memcpy(header, p, data_len);
        header = (uint8_t*)&(mov.traks_info_[index].sequence_data_[0]);
        memcpy(header, p, data_len);

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"data\":\"";
        uint8_t* p = (uint8_t*)&(data_[0]);

        ss << Data2HexString(p, data_.size());
        ss << "\"";
        ss << "}";
        return ss.str();
    }
public:
    std::vector<uint8_t> data_;

private:
    std::vector<uint8_t> serialize_data_;
};

class PaspBox : public Mp4BoxBase
{
public:
    PaspBox() { type_ = "pasp"; }
    ~PaspBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        h_spacing_ = ByteStream::Read4Bytes(p);
        p += 4;
        v_spacing_ = ByteStream::Read4Bytes(p);
        p += 4;

        return p;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"hspacing\":" << h_spacing_ << ",";
        ss << "\"vspacing\":" << v_spacing_;
        ss << "}";

        return ss.str();
    }

public:
    uint32_t h_spacing_ = 0;
    uint32_t v_spacing_ = 0;
};

class BtrtBox : public Mp4BoxBase
{
public:
    BtrtBox() {
        type_ = "btrt";
    }
    ~BtrtBox() {
    }

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        buffer_size_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].buffer_size_ = buffer_size_;
        p += 4;

        max_bit_rate_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].max_bit_rate_ = max_bit_rate_;
        p += 4;

        avg_bit_rate_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].avg_bit_rate_ = avg_bit_rate_;
        p += 4;

        assert(p == start + box_size_);

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"buffer_size\":" << buffer_size_ << ",";
        ss << "\"max_bit_rate\":" << max_bit_rate_ << ",";
        ss << "\"avg_bit_rate\":" << avg_bit_rate_;
        ss << "}";

        return ss.str();
    }
public:
    uint32_t buffer_size_;
    uint32_t max_bit_rate_;
    uint32_t avg_bit_rate_;
};

class EsdsBox : public Mp4BoxBase
{
public:
    EsdsBox() {
        type_ = "esds";
    }
    ~EsdsBox() {
    }

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_ = ByteStream::Read4Bytes(p);
        p += 4;

        es_descr_tag_ = *p;//default 0x03
        p++;
        es_descr_len_ = GetDescLen(p);//3 + 5+13 + decoder_specific_info_len + 5+1
        p += 4;

        es_id_ = ByteStream::Read2Bytes(p);
        p += 2;

        uint8_t flag = *p;
        p++;
        stream_dependence_flag_ = (flag >> 7) & 0x01;
        url_flag_ = (flag >> 6) & 0x01;
        ocr_stream_flag_ = (flag >> 5) & 0x01;
        stream_priority_ = flag & 0x1f;

        if (stream_dependence_flag_) {
            dependson_es_id_ = ByteStream::Read2Bytes(p);
            p += 2;
        }
        if (url_flag_) {
            url_length_ = *p++;
            url_string_ = *p++;
        }
        if (ocr_stream_flag_) {
            ocr_es_id_ = ByteStream::Read2Bytes(p);
            p += 2;
        }

        dec_conf_descr_tag_ = *p;//default: 0x04
        p++;
        decoder_specific_info_len_ = GetDescLen(p);//13 + decoder_specific_info_len
        p += 4;

        object_type_indication_ = *p;
        p++;

        flag = *p;
        streamtype_ = (flag >> 2) & 0x3f;
        upstream_ = (flag >> 1) & 0x01;
        p++;
        
        buffer_size_ = ByteStream::Read3Bytes(p);
        mov.traks_info_[index].buffer_size_ = buffer_size_;
        p += 3;

        maxbitrate_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].max_bit_rate_ = maxbitrate_;
        p += 4;

        avg_bit_rate_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].avg_bit_rate_ = avg_bit_rate_;
        p += 4;

        dec_specific_info_tag_ = *p;//0x05
        p++;
        extra_data_len_ = GetDescLen(p);
        p += 4;

        if (extra_data_len_ > 0) {
            extra_data_.resize(extra_data_len_);
            mov.traks_info_[index].sequence_data_.resize(extra_data_len_);

            uint8_t* ext_data = (uint8_t*)(&extra_data_[0]);

            memcpy(ext_data, p, extra_data_len_);
            ext_data = (uint8_t*)(&mov.traks_info_[index].sequence_data_[0]);
            memcpy(ext_data, p, extra_data_len_);
        }

        assert(p <= start + box_size_);
        return start + box_size_;
    }

    uint32_t GetDescLen(uint8_t* p) {
        uint32_t ret = 0;

        for (int i = 3; i > 0; i--) {
            uint8_t unit = *p;
            ret += (((uint32_t)unit) << (i*7)) & 0x7f;
            p++;
        }
        ret += *p & 0x7f;

        return ret;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << version_ << ",";

        ss << "\"es_descr_tag\":" << (int)es_descr_tag_ << ",";
        ss << "\"es_descr_len\":" << es_descr_len_ << ",";

        ss << "\"es_id\":" << es_id_ << ",";
        ss << "\"stream_dependence_flag\":" << (int)stream_dependence_flag_ << ",";
        ss << "\"url_flag\":" << (int)url_flag_ << ",";
        ss << "\"ocr_stream_flag\":" << (int)ocr_stream_flag_ << ",";
        ss << "\"stream_priority\":" << (int)stream_priority_ << ",";
        ss << "\"dependson_es_id_\":" << dependson_es_id_ << ",";

        if (url_flag_) {
            ss << "\"url_length\":" << (int)url_length_ << ",";
            ss << "\"url_string\":" << (int)url_string_ << ",";
        }
        if (ocr_stream_flag_) {
            ss << "\"ocr_es_id\":" << ocr_es_id_ << ",";
        }
        ss << "\"dec_conf_descr_tag\":" << (int)dec_conf_descr_tag_ << ",";
        ss << "\"decoder_specific_info_len\":" << decoder_specific_info_len_ << ",";
        ss << "\"object_type_indication\":" << (int)object_type_indication_ << ",";
        ss << "\"streamtype\":" << (int)streamtype_ << ",";
        ss << "\"upstream\":" << (int)upstream_ << ",";
        ss << "\"buffer_size\":" << buffer_size_ << ",";
        ss << "\"maxbitrate\":" << maxbitrate_ << ",";
        ss << "\"avg_bit_rate\":" << avg_bit_rate_ << ",";

        ss << "\"dec_specific_info_tag\":" << (int)dec_specific_info_tag_ << ",";
        ss << "\"extra_data_len\":" << extra_data_len_ << ",";

        ss << "\"extra_data\":[";
        
        size_t index = 0;
        for (uint8_t unit : extra_data_) {
            ss << (int)unit;
            index++;
            if (index < extra_data_.size()) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";
        return ss.str();
    }
public:
    uint32_t version_;
    
    //7.2.6.5 ES_Descriptor
    uint8_t es_descr_tag_ = 0x03;
    uint32_t es_descr_len_;
    
    uint16_t es_id_;
    uint8_t stream_dependence_flag_;//1bit
    uint8_t url_flag_;//1bit
    uint8_t ocr_stream_flag_;//1bit
    uint8_t stream_priority_;//5bits
    uint16_t dependson_es_id_;//if (stream_dependence_flag_ == 1)

    //if (url_flag_ == 1)
    uint8_t url_length_;
    uint8_t url_string_;

    //if (ocr_stream_flag_ == 1)
    uint16_t  ocr_es_id_;

    //7.2.6.6 DecoderConfigDescriptor in ISO_IEC_14496-1.pdf
    //DecoderConfigDescriptor(0x04) below
    uint8_t dec_conf_descr_tag_ = 0x04;
    uint32_t decoder_specific_info_len_;
    //DecoderConfigDescriptor items
    uint8_t object_type_indication_;
    uint8_t streamtype_;//6bits
    uint8_t upstream_;//1bit
    uint32_t buffer_size_;//24bits
    uint32_t maxbitrate_;
    uint32_t avg_bit_rate_;
    
    //DecoderSpecificInfo(0x05) below
    uint8_t dec_specific_info_tag_ = 0x05;
    uint32_t extra_data_len_;
    
    std::vector<uint8_t> extra_data_;

    uint8_t sl_descriptor_ = 0x06;
    uint32_t sl_descriptor_len_ = 1;
    uint8_t sl_flag_ = 0x02;
};

class Vp09Box : public Mp4BoxBase
{
public:
    Vp09Box() {
        type_ = "vp09";
    }
    ~Vp09Box() {
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (hex_data_.size() > 0) {
            memcpy(p, &(hex_data_[0]), hex_data_.size());
            p += hex_data_.size();
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        size_t data_len = box_size_ - (p - start);

        mov.traks_info_[index].codec_type_ = GetCodecTypeByBoxType(type_);
        hex_data_.resize(data_len);
        mov.traks_info_[index].sequence_data_.resize(data_len);
        uint8_t* header = (uint8_t*)&(hex_data_[0]);

        memcpy(header, p, data_len);
        header = (uint8_t*)&(mov.traks_info_[index].sequence_data_[0]);
        memcpy(header, p, data_len);

        return start + box_size_;
    }
public:
    std::vector<uint8_t> hex_data_;

private:
    std::vector<uint8_t> serialize_data_;
};

class OpusBox : public Mp4BoxBase
{
public:
    OpusBox() {
        type_ = "Opus";
    }
    ~OpusBox() {
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, 0);//reserve 4 bytes
        p += 4;
        ByteStream::Write2Bytes(p, 0);//reserve 2 bytes
        p += 2;
        ByteStream::Write2Bytes(p, data_ref_index_);
        p += 2;
        ByteStream::Write2Bytes(p, version_);
        p += 2;
        ByteStream::Write2Bytes(p, revision_level_);
        p += 2;
        ByteStream::Write4Bytes(p, 0);//reserve 4 bytes
        p += 4;
        ByteStream::Write2Bytes(p, channelcount_);
        p += 2;
        ByteStream::Write2Bytes(p, samplesize_);
        p += 2;
        ByteStream::Write2Bytes(p, pre_defined_);
        p += 2;
        ByteStream::Write2Bytes(p, 0);//reserve 2 bytes
        p += 2;
        ByteStream::Write4Bytes(p, samplerate_ << 16);
        p += 4;

        if (extra_data_.size() > 0) {
            memcpy(p, extra_data_.data(), extra_data_.size());
            p += extra_data_.size();
        }
        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        reserved1_ = ByteStream::Read4Bytes(p);
        p += 4;
        reserved2_ = ByteStream::Read2Bytes(p);
        p += 2;
        data_ref_index_ = ByteStream::Read2Bytes(p);
        p += 2;

        version_ = ByteStream::Read2Bytes(p);
        p += 2;
        revision_level_ = ByteStream::Read2Bytes(p);
        p += 2;

        reserved3_ = ByteStream::Read4Bytes(p);
        p += 4;

        channelcount_ = ByteStream::Read2Bytes(p);
        mov.traks_info_[index].channelcount_ = channelcount_;
        p += 2;

        samplesize_ = ByteStream::Read2Bytes(p);
        mov.traks_info_[index].samplesize_ = samplesize_;
        p += 2;

        pre_defined_ = ByteStream::Read2Bytes(p);
        p += 2;
        reserved4_ = ByteStream::Read2Bytes(p);
        p += 2;

        samplerate_ = ByteStream::Read4Bytes(p) >> 16;
        mov.traks_info_[index].samplerate_ = samplerate_;
        p += 4;

        assert(p < start + box_size_);

        if (p < start + box_size_) {
            int extra_len = start + box_size_ - p;
            extra_data_.resize(extra_len);

            memcpy(extra_data_.data(), p, extra_len);

            mov.traks_info_[index].sequence_data_.resize(extra_len);
            uint8_t* extra_data = (uint8_t*)&(mov.traks_info_[index].sequence_data_[0]);
            memcpy(extra_data, p, extra_len);
        }
        
        return start + box_size_;
    }
public:
    uint32_t reserved1_ = 0;
    uint16_t reserved2_ = 0;
    uint16_t data_ref_index_ = 1;
    uint16_t version_ = 0;
    uint16_t revision_level_ = 0;
    uint32_t reserved3_ = 0;
    uint16_t channelcount_ = 0;
    uint16_t samplesize_ = 0;
    uint16_t pre_defined_ = 0;
    uint16_t reserved4_ = 0;
    uint32_t samplerate_ = 0;
    std::vector<uint8_t> extra_data_;
    EsdsBox* esds_ = nullptr;
    BtrtBox* btrt_ = nullptr;
    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

class Mp4aBox : public Mp4BoxBase
{
public:
    Mp4aBox() {
        type_ = "mp4a";
    }
    ~Mp4aBox() {
        if (esds_) {
            delete esds_;
            esds_ = nullptr;
        }
        if (btrt_) {
            delete btrt_;
            btrt_ = nullptr;
        }

        for (Mp4BoxBase* box : unknown_boxes_) {
            delete box;
        }
        unknown_boxes_.clear();
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, 0);//reserve 4 bytes
        p += 4;
        ByteStream::Write2Bytes(p, 0);//reserve 2 bytes
        p += 2;
        ByteStream::Write2Bytes(p, data_ref_index_);
        p += 2;
        ByteStream::Write2Bytes(p, version_);
        p += 2;
        ByteStream::Write2Bytes(p, revision_level_);
        p += 2;
        ByteStream::Write4Bytes(p, 0);//reserve 4 bytes
        p += 4;
        ByteStream::Write2Bytes(p, channelcount_);
        p += 2;
        ByteStream::Write2Bytes(p, samplesize_);
        p += 2;
        ByteStream::Write2Bytes(p, pre_defined_);
        p += 2;
        ByteStream::Write2Bytes(p, 0);//reserve 2 bytes
        p += 2;
        ByteStream::Write4Bytes(p, samplerate_);
        p += 4;

        if (extra_data_.size() > 0) {
            memcpy(p, extra_data_.data(), extra_data_.size());
            p += extra_data_.size();
        }
        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        reserved1_ = ByteStream::Read4Bytes(p);
        p += 4;
        reserved2_ = ByteStream::Read2Bytes(p);
        p += 2;
        data_ref_index_ = ByteStream::Read2Bytes(p);
        p += 2;

        version_ = ByteStream::Read2Bytes(p);
        p += 2;
        revision_level_ = ByteStream::Read2Bytes(p);
        p += 2;

        reserved3_ = ByteStream::Read4Bytes(p);
        p += 4;

        channelcount_ = ByteStream::Read2Bytes(p);
        mov.traks_info_[index].channelcount_ = channelcount_;
        p += 2;

        samplesize_ = ByteStream::Read2Bytes(p);
        mov.traks_info_[index].samplesize_ = samplesize_;
        p += 2;

        pre_defined_ = ByteStream::Read2Bytes(p);
        p += 2;
        reserved4_ = ByteStream::Read2Bytes(p);
        p += 2;

        samplerate_ = ByteStream::Read4Bytes(p) >> 16;
        mov.traks_info_[index].samplerate_ = samplerate_;
        p += 4;

        assert(p < start + box_size_);

        if (p < start + box_size_) {
            int extra_len = start + box_size_ - p;
            extra_data_.resize(extra_len);

            memcpy(extra_data_.data(), p, extra_len);

            mov.traks_info_[index].sequence_data_.resize(extra_len);
            uint8_t* extra_data = (uint8_t*)&(mov.traks_info_[index].sequence_data_[0]);
            memcpy(extra_data, p, extra_len);
        }
        
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"data_ref_index\":" << data_ref_index_ << ",";
        ss << "\"version\":" << version_ << ",";
        ss << "\"revision_level\":" << revision_level_ << ",";
        ss << "\"channelcount\":" << channelcount_ << ",";
        ss << "\"samplesize\":" << samplesize_ << ",";
        ss << "\"pre_defined\":" << pre_defined_ << ",";
        ss << "\"samplerate\":" << samplerate_;
        if (esds_) {
            ss << ",";
            ss << "\"esds\":" << esds_->Dump();
        }
        if (btrt_) {
            ss << ",";
            ss << "\"btrt\":" << btrt_->Dump();
        }
        if (unknown_boxes_.size() > 0) {
            size_t index = 0;
            ss << ",";
            for (Mp4BoxBase* box : unknown_boxes_) {
                ss << box->Dump();
                index++;

                if (index < unknown_boxes_.size()) {
                    ss << ",";
                }
            }
        }
        ss << "}";
        return ss.str();
    }

public:
    uint32_t reserved1_ = 0;
    uint16_t reserved2_ = 0;
    uint16_t data_ref_index_ = 1;
    uint16_t version_ = 0;
    uint16_t revision_level_ = 0;
    uint32_t reserved3_ = 0;
    uint16_t channelcount_ = 0;
    uint16_t samplesize_ = 0;
    uint16_t pre_defined_ = 0;
    uint16_t reserved4_ = 0;
    uint32_t samplerate_ = 0;
    std::vector<uint8_t> extra_data_;
    EsdsBox* esds_ = nullptr;
    BtrtBox* btrt_ = nullptr;
    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

class StsdAvcInfo
{
public:
    StsdAvcInfo()
    {
    }
    ~StsdAvcInfo()
    {
    }

public:
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, reserved1_);
        p += 4;
        ByteStream::Write2Bytes(p, reserved2_);
        p += 2;
        ByteStream::Write2Bytes(p, data_reference_index_);
        p += 2;
        ByteStream::Write2Bytes(p, codec_stream_version_);
        p += 2;
        ByteStream::Write2Bytes(p, codec_stream_reversion_);
        p += 2;

        for (size_t i = 0; i < sizeof(reserved3_)/sizeof(uint32_t); i++) {
            ByteStream::Write4Bytes(p, reserved3_[i]);
            p += 4;
        }
        ByteStream::Write2Bytes(p, width_);
        p += 2;
        ByteStream::Write2Bytes(p, height_);
        p += 2;

        ByteStream::Write4Bytes(p, horizontal_resolution_);
        p += 4;
        ByteStream::Write4Bytes(p, vertical_resolution_);
        p += 4;

        ByteStream::Write4Bytes(p, data_size_);
        p += 4;
        ByteStream::Write2Bytes(p, frame_count_);
        p += 2;

        for (size_t i = 0; i < sizeof(compressorname_); i++) {
            *p = (uint8_t)compressorname_[i];
            p++;
        }
        ByteStream::Write2Bytes(p, alpha_);
        p += 2;
        ByteStream::Write2Bytes(p, reserved4_);
        p += 2;

        len = p - data;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* data, MovInfo& mov) {
        uint8_t* p = data;
        size_t index = mov.traks_info_.size() - 1;

        reserved1_ = ByteStream::Read4Bytes(p);
        p += 4;
        reserved2_ = ByteStream::Read2Bytes(p);
        p += 2;
        data_reference_index_ = ByteStream::Read2Bytes(p);
        p += 2;
        codec_stream_version_ = ByteStream::Read2Bytes(p);
        p += 2;
        codec_stream_reversion_ = ByteStream::Read2Bytes(p);
        p += 2;

        for (size_t i = 0; i < sizeof(reserved3_)/sizeof(uint32_t); i++) {
            reserved3_[i] = ByteStream::Read4Bytes(p);
            p += 4;
        }
        width_ = ByteStream::Read2Bytes(p);
        mov.traks_info_[index].width_ = width_;
        p += 2;
        height_ = ByteStream::Read2Bytes(p);
        mov.traks_info_[index].height_ = height_;
        p += 2;

        horizontal_resolution_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].horizontal_resolution_ = horizontal_resolution_;
        p += 4;
        vertical_resolution_ = ByteStream::Read4Bytes(p);
        mov.traks_info_[index].vertical_resolution_ = vertical_resolution_;
        p += 4;

        data_size_ = ByteStream::Read4Bytes(p);
        p += 4;
        frame_count_ = ByteStream::Read2Bytes(p);
        p += 2;

        for (size_t i = 0; i < sizeof(compressorname_); i++) {
            compressorname_[i] = (char)(*p);
            p++;
        }
        alpha_ = ByteStream::Read2Bytes(p);
        p += 2;
        reserved4_ = ByteStream::Read2Bytes(p);
        p += 2;

        return p;
    }

    std::string Dump(std::stringstream& ss) {
        ss << "\"reserved1\":" << reserved1_ << ",";
        ss << "\"reserved2\":" << reserved2_ << ",";
        ss << "\"data_reference_index\":" << data_reference_index_ << ",";
        ss << "\"codec_stream_version\":" << codec_stream_version_ << ",";
        ss << "\"codec_stream_reversion\":" << codec_stream_reversion_ << ",";
        ss << "\"reserved3\":[";
        for (size_t i = 0; i < sizeof(reserved3_)/sizeof(uint32_t); i++) {
            ss << reserved3_[i];
            if (i != (sizeof(reserved3_)/sizeof(uint32_t) - 1)) {
                ss << ",";
            }
        }
        ss << "],";
        ss << "\"width\":" << width_ << ",";
        ss << "\"height\":" << height_ << ",";
        ss << "\"horizontal_resolution\":" << horizontal_resolution_ << ",";
        ss << "\"vertical_resolution_\":" << vertical_resolution_ << ",";
        ss << "\"data_size\":" << data_size_ << ",";
        ss << "\"frame_count\":" << frame_count_ << ",";
        
        std::string compressorname = DataToString(compressorname_, sizeof(compressorname_));

        if (!compressorname.empty()) {
            ss << "\"compressorname\":\"" << compressorname << "\",";
        }
        ss << "\"alpha\":" << alpha_ << ",";
        ss << "\"reserved4\":" << reserved4_ << ",";
        return ss.str();
    }
public:
    uint32_t reserved1_ = 0;
    uint16_t reserved2_ = 0;
    uint16_t data_reference_index_ = 0;
    uint16_t codec_stream_version_ = 0;//Reserved
    uint16_t codec_stream_reversion_ = 0;//Reserved
    uint32_t reserved3_[3] = {0};
    uint16_t width_ = 0;
    uint16_t height_ = 0;
    uint32_t horizontal_resolution_ = 0;
    uint32_t vertical_resolution_ = 0;
    uint32_t data_size_ = 0;//Reserved
    uint16_t frame_count_ = 0;//frame cout == 1
    char compressorname_[32] = {0};
    uint16_t alpha_ = 0x18;
    uint16_t reserved4_ = 0;

private:
    std::vector<uint8_t> serialize_data_;
};

class LhvCBox : public Mp4BoxBase
{
public:
    LhvCBox() {
        type_ = "lhvC";
    }
    ~LhvCBox() {
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (hex_data_.size() > 0) {
            memcpy(p, &(hex_data_[0]), hex_data_.size());
            p += hex_data_.size();
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;
        size_t extra_data_len = box_size_- (p - start);

        hex_data_.resize(extra_data_len);
        memcpy(hex_data_.data(), p, extra_data_len);
        GetLHevcDecInfoFromExtradata(&lhvC_spspps_, p, extra_data_len);
        mov.traks_info_[index].lhevc_dec_conf_ = lhvC_spspps_;

        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"seq_headers\":" << LHevcDecInfoDump(&lhvC_spspps_);
        ss << "}";
        return ss.str();
    }
public:
    std::vector<uint8_t> hex_data_;
    LHEVC_DEC_CONF_RECORD lhvC_spspps_;

private:
    std::vector<uint8_t> serialize_data_;
};

class HvcCBox : public Mp4BoxBase
{
public:
    HvcCBox() {
        type_ = "hvcC";
    }
    ~HvcCBox() {
    }
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (hex_data_.size() > 0) {
            // std::cout << "hvcC hex data len: " << hex_data_.size() << std::endl;
            // std::cout << "hvcC hex data:" << Data2HexString(&(hex_data_[0]), hex_data_.size()) << std::endl;
            memcpy(p, &(hex_data_[0]), hex_data_.size());
            p += hex_data_.size();
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;
        size_t extra_data_len = box_size_- (p - start);

        // std::cout << "hvcC extra_data_len: " << extra_data_len << std::endl;
        // std::cout << "hvcC hex data:" << Data2HexString(p, extra_data_len) << std::endl;
        if (extra_data_len > 5 * 1024) {
            CSM_THROW_ERROR("try get hevc decode info exception, extra_data_len(%lu) is too large", extra_data_len);
        }
        int ret = GetHevcDecInfoFromExtradata(&hevc_dec_info_, p, extra_data_len);
        if (ret < 0) {
            CSM_THROW_ERROR("try get hevc decode info exception");
        }

        mov.traks_info_[index].codec_type_ = GetCodecTypeByBoxType(type_);

        hex_data_.resize(extra_data_len);
        uint8_t* hex_data_p = (uint8_t*)(&hex_data_[0]);
        memcpy(hex_data_p, p, extra_data_len);

        mov.traks_info_[index].sequence_data_.resize(extra_data_len);
        uint8_t* seq_data  = (uint8_t*)&(mov.traks_info_[index].sequence_data_[0]);
        memcpy(seq_data, p, extra_data_len);

        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;
        uint8_t* p = (uint8_t*)&(hex_data_[0]);

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"decinfo\":" << HevcDecInfoDump(&hevc_dec_info_);
        ss << ",";

        ss << "\"hex_data\":\"" << Data2HexString(p, hex_data_.size()) << "\"";
        ss << "}";

        return ss.str();
    }
public:
    HEVC_DEC_CONF_RECORD hevc_dec_info_;
    std::vector<uint8_t> hex_data_;

private:
    std::vector<uint8_t> serialize_data_;
};

class StriBox : public Mp4BoxBase
{
public:
    StriBox() {
        type_ = "stri";
    }
    ~StriBox() {
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;

        uint8_t value = *p;

        eye_views_reversed_ = (value & 0x08) ? true : false;
        has_additional_views_ = (value & 0x04) ? true : false;
        has_right_eye_view_ = (value & 0x02) ? true : false;
        has_left_eye_view_ = (value & 0x01) ? true : false;

        return start + box_size_;
    }
public:
    uint32_t version_flag_ = 0;
    bool eye_views_reversed_ = false;
    bool has_additional_views_ = false;
    bool has_right_eye_view_ = false;
    bool has_left_eye_view_ = false;
};

class EyesBox : public Mp4BoxBase
{
public:
    EyesBox() {
        type_ = "eyes";
    }
    ~EyesBox() {
    }

public:
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        while (p < (start + box_size_ - 6)) {
            std::string box_type;
            int offset;

            size_t box_len = GetBoxHeaderInfo(p, box_type, offset);
            (void)box_len;

            if (box_type == "stri") {
                stri_ = new StriBox();
                p = stri_->Parse(p, mov);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                p += box->box_size_;
                unknown_boxes_.push_back(box);
            }
        }
        return start + box_size_;
    }

public:
    StriBox* stri_ = nullptr;
    std::vector<Mp4BoxBase*> unknown_boxes_;
};

class MustBox : public Mp4BoxBase
{
public:
    MustBox() {
        type_ = "must";
    }
    ~MustBox() {
    }

public:
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        (void)p;
        return start + box_size_;
    }
};

class VexuBox : public Mp4BoxBase
{
public:
    VexuBox() {
        type_ = "vexu";
    }
    ~VexuBox() {
        if (eyes_) {
            delete eyes_;
            eyes_ = nullptr;
        }
    }

public:
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (hex_data_.size() > 0) {
            memcpy(p, &(hex_data_[0]), hex_data_.size());
            p += hex_data_.size();
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t extra_data_len = box_size_ - (p - start);

        hex_data_.resize(extra_data_len);
        memcpy(hex_data_.data(), p, extra_data_len);

        while (p < (start + box_size_ - 6)) {
            std::string box_type;
            int offset;

            size_t box_len = GetBoxHeaderInfo(p, box_type, offset);
            (void)box_len;
            if (box_type == "eyes") {
                eyes_ = new EyesBox();
                p = eyes_->Parse(p, mov);
            } else if (box_type == "must") {
                must_ = new MustBox();
                p = must_->Parse(p, mov);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                p += box->box_size_;
                unknown_boxes_.push_back(box);
            }
        }
        return start + box_size_;
    }

public:
    std::vector<uint8_t> hex_data_;
    EyesBox* eyes_ = nullptr;
    MustBox* must_ = nullptr;
    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

class Hvc1Box : public Mp4BoxBase, public StsdAvcInfo
{
public:
    Hvc1Box() {
        type_ = "hvc1";
    }
    ~Hvc1Box() {
        if (hvcC_) {
            delete hvcC_;
            hvcC_ = nullptr;
        }
        if (lhvC_) {
            delete lhvC_;
            lhvC_ = nullptr;
        }
        for (Mp4BoxBase* box : unknown_boxes_) {
            delete box;
        }
        unknown_boxes_.clear();
    }
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        size_t avc_info_len = 0;
        uint8_t* avc_info_data = StsdAvcInfo::Serialize(avc_info_len);

        memcpy(p, avc_info_data, avc_info_len);
        p += avc_info_len;
        delete[] avc_info_data;

        if (hvcC_) {
            size_t box_size = 0;
            uint8_t* box_data = hvcC_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (lhvC_) {
            size_t box_size = 0;
            uint8_t* box_data = lhvC_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (vexu_) {
            size_t box_size = 0;
            uint8_t* box_data = vexu_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        p = StsdAvcInfo::Parse(p, mov);
        //next box: avcC(h264), hvcC(h265), av1C(av1), vvcC(h266), vpcC(vp8, vp9)

        while (p < (start + box_size_ - 6)) {
            std::string box_type;
            int offset;

            (void)GetBoxHeaderInfo(p, box_type, offset);
            // std::cout << "hvc1 box get sub box_type:" << box_type << std::endl;
            if (box_type == "hvcC") {
                hvcC_ = new HvcCBox();
                p = hvcC_->Parse(p, mov);
            } else if (box_type == "lhvC") {
                lhvC_ = new LhvCBox();
                p = lhvC_->Parse(p, mov);
            } else if (box_type == "vexu") {
                vexu_ = new VexuBox();
                p = vexu_->Parse(p, mov);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                p += box->box_size_;
                unknown_boxes_.push_back(box);
            }
        }
        
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        StsdAvcInfo::Dump(ss);
        if (hvcC_) {
            ss << "\"hvcC\":" << hvcC_->Dump();
        }
        if (lhvC_) {
            if (hvcC_) {
                ss << ",";
            }
            ss << "\"lhvC\":" << lhvC_->Dump();
        }
        for (Mp4BoxBase* unknown_box : unknown_boxes_) {
            ss << ",";
            ss << "\"" << unknown_box->type_ << "\":" << unknown_box->Dump();
        }
        ss << "}";
        return ss.str();
    }

public:
    HvcCBox* hvcC_ = nullptr;
    LhvCBox* lhvC_ = nullptr;
    VexuBox* vexu_ = nullptr;
    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

class Avc1Box : public Mp4BoxBase, public StsdAvcInfo
{
public:
    Avc1Box() {
        type_ = "avc1";
        memset(compressorname_, 0, sizeof(compressorname_));
    }
    ~Avc1Box() {
        if (video_hdr_box_) {
            delete video_hdr_box_;
            video_hdr_box_ = nullptr;
        }
        if (pasp_box_) {
            delete pasp_box_;
            pasp_box_ = nullptr;
        }
        for (Mp4BoxBase* box : unknown_boxes_) {
            delete box;
        }
        unknown_boxes_.clear();
    }
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (video_hdr_box_) {
            size_t box_size = 0;
            uint8_t* box_data = video_hdr_box_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)box_size_);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        p = StsdAvcInfo::Parse(p, mov);
        //next box: avcC(h264), hvcC(h265), av1C(av1), vvcC(h266), vpcC(vp8, vp9)
        video_hdr_box_ = new VideoSequenceBox();
        p = video_hdr_box_->Parse(p, mov);

        if(p == start + box_size_) {
            return start + box_size_;
        }

        while (p < start + box_size_) {
            std::string box_type;
            int offset = 0;
            GetBoxHeaderInfo(p, box_type, offset);
            if (box_type == "pasp") {
                pasp_box_ = new PaspBox();
                p = pasp_box_->Parse(p);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                p += box->box_size_;

                unknown_boxes_.push_back(box);
            }
        }

        assert(p == start + box_size_);

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        StsdAvcInfo::Dump(ss);
        ss << "\"video_hdr\":" << video_hdr_box_->Dump();
        if (pasp_box_) {
            ss << ",";
            ss << "\"pasp\":" << pasp_box_->Dump();
        }
        if (unknown_boxes_.size() > 0) {
            ss << ",";
            size_t index = 0;
            for (Mp4BoxBase* box : unknown_boxes_) {
                ss << box->Dump();
                index++;
                if (index < unknown_boxes_.size()) {
                    ss << ",";
                }
            }
        }
        ss << "}";

        return ss.str();
    }

public:
    VideoSequenceBox* video_hdr_box_ = nullptr;
    PaspBox* pasp_box_ = nullptr;
    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

//stsd is in stbl, 
class StsdBox : public Mp4BoxBase
{
public:
    StsdBox() {
        type_ = "stsd";
    }
    ~StsdBox() {
        if (avc1_box_) {
            delete avc1_box_;
            avc1_box_ = nullptr;
        }
        if (hvc1_box_) {
            delete hvc1_box_;
            hvc1_box_ = nullptr;
        }
        if (mp4a_box_) {
            delete mp4a_box_;
            mp4a_box_ = nullptr;
        }

        for (Mp4BoxBase* box : unknown_boxes_) {
            delete box;
        }
        unknown_boxes_.clear();
    }
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, entry_count_);
        p += 4;

        if (avc1_box_) {
            size_t box_size = 0;
            uint8_t* box_data = avc1_box_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (hvc1_box_) {
            size_t box_size = 0;
            uint8_t* box_data = hvc1_box_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (vp09_box_) {
            size_t box_size = 0;
            uint8_t* box_data = vp09_box_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (mp4a_box_) {
            size_t box_size = 0;
            uint8_t* box_data = mp4a_box_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (opus_box_) {
            size_t box_size = 0;
            uint8_t* box_data = opus_box_->Serialize(box_size);
            // std::cout << "opus box size:" << box_size << std::endl;
            memcpy(p, box_data, box_size);
            p += box_size;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)box_size_);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        entry_count_ = ByteStream::Read4Bytes(p);
        p += 4;

        for (size_t i = 0; i < entry_count_; i++) {
            uint64_t media_box_size;
            std::string media_box_type;
            int offset = 0;

            media_box_size = GetBoxHeaderInfo(p, media_box_type, offset);
            assert(media_box_size < box_size_);

            // std::cout << "parse media box:" << media_box_type << ", size:" << media_box_size << std::endl;
            // std::cout << "handle type:" << mov.traks_info_[index].handler_type_ << std::endl;
            box_type_vec_.push_back(media_box_type);
            if (mov.traks_info_[index].handler_type_ == "vide") {
                if (media_box_type == "avc1") {
                    avc1_box_ = new Avc1Box();
                    p = avc1_box_->Parse(p, mov);
                } else if (media_box_type == "hvc1" || media_box_type == "hev1") {
                    hvc1_box_ = new Hvc1Box();
                    p = hvc1_box_->Parse(p, mov);
                } else if (media_box_type == "vp09") {
                    vp09_box_ = new Vp09Box();
                    p = vp09_box_->Parse(p, mov);
                } else {
                    Mp4BoxBase* box = new Mp4BoxBase();
                    box->Parse(p);
                    p = p + box->box_size_;
                    unknown_boxes_.push_back(box);
                }
            } else if (mov.traks_info_[index].handler_type_ == "soun") {
                mov.traks_info_[index].codec_type_ = GetCodecTypeByBoxType(media_box_type);
                if (media_box_type == "mp4a") {
                    mp4a_box_ = new Mp4aBox();
                    p = mp4a_box_->Parse(p, mov);
                } else if (media_box_type == "Opus") {
                    opus_box_ = new OpusBox();
                    p = opus_box_->Parse(p, mov);
                } else {
                    Mp4BoxBase* box = new Mp4BoxBase();
                    box->Parse(p);
                    p = p + box->box_size_;
                    unknown_boxes_.push_back(box);
                }
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                p = p + box->box_size_;
                unknown_boxes_.push_back(box);
            }
            assert(p <= start + box_size_);
        }
        assert(p == start + box_size_);

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"entry_count\":" << (entry_count_ & 0xffffff);

        for (size_t i = 0; i < entry_count_; i++) {
            ss << ",";
            if (box_type_vec_[i] == "avc1" && avc1_box_ != nullptr) {
                ss << "\"avc1\":" << avc1_box_->Dump();
                continue;
            };
            if ((box_type_vec_[i] == "hvc1" || box_type_vec_[i] == "hev1") && hvc1_box_ != nullptr) {
                ss << "\"" << box_type_vec_[i] << "\":" << hvc1_box_->Dump();
                continue;
            };
            if (box_type_vec_[i] == "mp4a" && mp4a_box_ != nullptr) {
                ss << "\"mp4a\":" << mp4a_box_->Dump();
                continue;
            }
            ss << "\"unknown_box_type\":\"" << box_type_vec_[i] << "\"";
        }
        ss << "}";
        
        return ss.str();
    }

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<std::string> box_type_vec_;
    Avc1Box* avc1_box_ = nullptr;
    Hvc1Box* hvc1_box_ = nullptr;
    Vp09Box* vp09_box_ = nullptr;
    Mp4aBox* mp4a_box_ = nullptr;
    OpusBox* opus_box_ = nullptr;

    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

//stts is in stbl
class SttsBox : public Mp4BoxBase
{
public:
    SttsBox() { type_ = "stts"; }
    ~SttsBox() {}
public:
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, entry_count_);
        p += 4;

        for (const auto& entry : sample_entries_) {
            ByteStream::Write4Bytes(p, entry.sample_count_);
            p += 4;
            ByteStream::Write4Bytes(p, entry.samples_delta_);
            p += 4;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)box_size_);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;

        entry_count_ = ByteStream::Read4Bytes(p);
        p += 4;

        for (size_t i = 0; i < entry_count_; i++) {
            SampleEntry entry;
            entry.sample_count_ = ByteStream::Read4Bytes(p);
            p += 4;
            entry.samples_delta_ = ByteStream::Read4Bytes(p);
            p += 4;
            sample_entries_.push_back(entry);
            mov.traks_info_[index].sample_entries_.push_back(entry);
            assert(p <= start + box_size_);
        }

        assert(p == start + box_size_);
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << ((version_flag_ >> 24) & 0xff) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"entry_count\":" << entry_count_;

        if (entry_count_ > 0) {
            ss << ",";
            ss << "\"entries\": [";
            for (size_t i = 0; i < entry_count_; i++) {
                ss << "{";
                ss << "\"sample_count\":" << sample_entries_.at(i).sample_count_ << ",";
                ss << "\"samples_delta\":" << sample_entries_.at(i).samples_delta_;
                ss << "}";
                if (i < entry_count_ - 1) {
                    ss << ",";
                }
            }
            ss << "]";
        }
        ss << "}";
        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<SampleEntry> sample_entries_;

private:
    std::vector<uint8_t> serialize_data_;
};

//stss is in stbl: I frame sample position list
class StssBox : public Mp4BoxBase
{
public:
    StssBox() { type_ = "stss"; }
    ~StssBox() {}

public:
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        entry_count_ = ByteStream::Read4Bytes(p);
        p += 4;

        for (size_t i = 0; i < entry_count_; i++) {
            uint32_t samples = ByteStream::Read4Bytes(p);
            p += 4;

            iframe_sample_vec_.push_back(samples);
            mov.traks_info_[index].iframe_sample_vec_.push_back(samples);
        }
        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << ((version_flag_ >> 24) & 0xff) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"entry_count\":" << entry_count_;

        if (entry_count_ > 0) {
            ss << ",";
            ss << "\"iframe_samples\":[";
            for (size_t i = 0; i < entry_count_; i++) {
                ss << iframe_sample_vec_[i];
                if (i < entry_count_ - 1) {
                    ss << ",";
                }
            }
            ss << "]";
        }
        ss << "}";
        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<uint32_t> iframe_sample_vec_;
};

//ctts is in stbl
class CttsBox : public Mp4BoxBase
{
public:
    CttsBox() { type_ = "ctts"; }
    ~CttsBox() {}

public:
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        entry_count_ = ByteStream::Read4Bytes(p);
        p += 4;

        for (size_t i = 0; i < entry_count_; i++) {
            SampleOffset sample_cts;

            sample_cts.sample_counts_ = ByteStream::Read4Bytes(p);
            p += 4;
            sample_cts.sample_offsets_ = ByteStream::Read4Bytes(p);
            p += 4;

            sample_offset_vec_.push_back(sample_cts);
            mov.traks_info_[index].sample_offset_vec_.push_back(sample_cts);

            assert(p <= start + box_size_);
        }
        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << ((version_flag_ >> 24) & 0xff) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"entry_count\":" << entry_count_;

        if (entry_count_ > 0) {
            ss << ",";
            ss << "\"cts_offsets\":[";
            for (size_t i = 0; i < entry_count_; i++) {
                ss << "{";
                ss << "\"sample_count\":" << sample_offset_vec_[i].sample_counts_ << ",";
                ss << "\"sample_offsets\":" << sample_offset_vec_[i].sample_offsets_;
                ss << "}";
                if (i < entry_count_ - 1) {
                    ss << ",";
                }
            }
            ss << "]";
        }
        ss << "}";
        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;

    std::vector<SampleOffset> sample_offset_vec_;
};

//stsc is in stbl
class StscBox : public Mp4BoxBase
{
public:
    StscBox() { type_ = "stsc"; }
    ~StscBox() {}
public:
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, entry_count_);
        p += 4;
        for (const auto& entry : chunk_sample_vec_) {
            ByteStream::Write4Bytes(p, entry.first_chunk_);
            p += 4;
            ByteStream::Write4Bytes(p, entry.samples_per_chunk_);
            p += 4;
            ByteStream::Write4Bytes(p, entry.sample_description_index_);
            p += 4;
        }
        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)box_size_);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        entry_count_ = ByteStream::Read4Bytes(p);
        p += 4;

        for (size_t i = 0; i < entry_count_; i++) {
            if (p >= start + box_size_) {
                break;
            }
            ChunkSample cs;

            cs.first_chunk_ = ByteStream::Read4Bytes(p);
            p += 4;
            cs.samples_per_chunk_ = ByteStream::Read4Bytes(p);
            p += 4;
            cs.sample_description_index_ = ByteStream::Read4Bytes(p);
            p += 4;

            chunk_sample_vec_.push_back(cs);
            mov.traks_info_[index].chunk_sample_vec_.push_back(cs);
        }
        assert(p == start + box_size_);

        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << ((version_flag_ >> 24) & 0xff) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"entry_count\":" << entry_count_;

        if (entry_count_ > 0) {
            ss << ",";
            ss << "\"chunk_samples\":[";
            for (size_t i = 0; i < entry_count_; i++) {
                ss << "{";
                ss << "\"first_chunk\":" << chunk_sample_vec_[i].first_chunk_ << ",";
                ss << "\"samples_per_chunk\":" << chunk_sample_vec_[i].samples_per_chunk_ << ",";
                ss << "\"sample_desc_index\":" << chunk_sample_vec_[i].sample_description_index_;
                ss << "}";
                if (i < entry_count_ - 1) {
                    ss << ",";
                }
            }
            ss << "]";
        }
        ss << "}";
        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<ChunkSample> chunk_sample_vec_;

private:
    std::vector<uint8_t> serialize_data_;
};

//stsz is in stbl
class StszBox : public Mp4BoxBase
{
public:
    StszBox() { type_ = "stsz"; }
    ~StszBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, constant_size_);
        p += 4;
        ByteStream::Write4Bytes(p, sample_count_);
        p += 4;

        for (const auto& size : sample_sizes_vec_) {
            ByteStream::Write4Bytes(p, size);
            p += 4;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)box_size_);

        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;

        constant_size_ = ByteStream::Read4Bytes(p);
        p += 4;

        sample_count_ = ByteStream::Read4Bytes(p);
        p += 4;

        if (constant_size_ == 0) {
            sample_sizes_vec_.resize(sample_count_);
            mov.traks_info_[index].sample_sizes_vec_.resize(sample_count_);
            for (size_t i = 0; i < sample_count_; i++) {
                sample_sizes_vec_[i] = ByteStream::Read4Bytes(p);
                mov.traks_info_[index].sample_sizes_vec_[i] = sample_sizes_vec_[i];
                p += 4;
            }
        }
        assert(p == start + box_size_);

        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << ((version_flag_ >> 24) & 0xff) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";

        ss << "\"constant_size\":" << constant_size_ << ",";
        ss << "\"sample_count\":" << sample_count_;

        if (constant_size_ == 0) {
            ss << ",";
            ss << "\"samples_size\":[";
            for (size_t i = 0; i < sample_count_; i++) {
                ss << sample_sizes_vec_[i];
                if (i < sample_count_ - 1) {
                    ss << ",";
                }
            }
            ss << "]";
        }
        ss << "}";
        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t constant_size_ = 0;
    uint32_t sample_count_ = 0;
    std::vector<uint32_t> sample_sizes_vec_;

private:
    std::vector<uint8_t> serialize_data_;
};

//stco is in stbl
class StcoBox : public Mp4BoxBase
{
public:
    StcoBox() { type_ = "stco"; }
    ~StcoBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, entry_count_);
        p += 4;

        for (const auto& offset : chunk_offsets_vec_) {
            ByteStream::Write4Bytes(p, offset);
            p += 4;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)box_size_);

        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        size_t index = mov.traks_info_.size() - 1;

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;

        entry_count_ = ByteStream::Read4Bytes(p);
        p += 4;

        chunk_offsets_vec_.resize(entry_count_);
        mov.traks_info_[index].chunk_offsets_vec_.resize(entry_count_);

        for (size_t i = 0; i < entry_count_; i++) {
            chunk_offsets_vec_[i] = ByteStream::Read4Bytes(p);
            mov.traks_info_[index].chunk_offsets_vec_[i] = chunk_offsets_vec_[i];
            p += 4;
        }
        assert(p == start + box_size_);

        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << ((version_flag_ >> 24) & 0xff) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";

        ss << "\"entry_count\":" << entry_count_;
        if (entry_count_) {
            ss << ",";
            ss << "\"chunk_offsets\":";
            ss << "[";
            for (size_t i = 0; i < entry_count_; i++) {
                ss << chunk_offsets_vec_[i];
                if (i < entry_count_ - 1) {
                    ss << ",";
                }
            }
            ss << "]";
        }

        ss << "}";
        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<uint32_t> chunk_offsets_vec_;

private:
    std::vector<uint8_t> serialize_data_;
};

//stbl is in minf. it has stsd, stts, stsc, stsz, stco, sgpd, sbgp
class StblBox : public Mp4BoxBase
{
public:
    StblBox() {
        type_ = "stbl";
    }
    ~StblBox() {
        if (stsd_) {
            delete stsd_;
            stsd_ = nullptr;
        }
        if (stts_) {
            delete stts_;
            stts_ = nullptr;
        }
        if (ctts_) {
            delete ctts_;
            ctts_ = nullptr;
        }
        if (stss_) {
            delete stss_;
            stss_ = nullptr;
        }
        if (stsc_) {
            delete stsc_;
            stsc_ = nullptr;
        }
        if (stsz_) {
            delete stsz_;
            stsz_ = nullptr;
        }
        if (stco_) {
            delete stco_;
            stco_ = nullptr;
        }
        for (Mp4BoxBase* box : unknown_boxes_) {
            delete box;
        }
        unknown_boxes_.clear();
    }
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (stsd_) {
            size_t box_size = 0;
            uint8_t* box_data = stsd_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (stts_) {
            size_t box_size = 0;
            uint8_t* box_data = stts_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (stsc_) {
            size_t box_size = 0;
            uint8_t* box_data = stsc_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (stsz_) {
            size_t box_size = 0;
            uint8_t* box_data = stsz_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (stco_) {
            size_t box_size = 0;
            uint8_t* box_data = stco_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, box_size_);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while(p < start + box_size_) {
            GetBoxHeaderInfo(p, box_type, offset);
            // std::cout << "stbl box_type: " << box_type << std::endl;
            if (box_type == "stsd") {
                stsd_ = new StsdBox();
                p = stsd_->Parse(p, mov);
            } else if (box_type == "stts") {
                stts_ = new SttsBox();
                p = stts_->Parse(p, mov);
            } else if (box_type == "ctts") {
                ctts_ = new CttsBox();
                p = ctts_->Parse(p, mov);
            } else if (box_type == "stss") {
                stss_ = new StssBox();
                p = stss_->Parse(p, mov);
            } else if (box_type == "stsc") {
                stsc_ = new StscBox();
                p = stsc_->Parse(p, mov);
            } else if (box_type == "stsz") {
                stsz_ = new StszBox();
                p = stsz_->Parse(p, mov);
            } else if (box_type == "stco") {
                stco_ = new StcoBox();
                p = stco_->Parse(p, mov);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                unknown_boxes_.push_back(box);
                p += box->box_size_;
            }
        }
        assert(p == start + box_size_);
        return start + box_size_;
    }
    
    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_;

        if (stsd_) {
            ss << ",";
            ss << "\"stsd\":" << stsd_->Dump();
        }

        if (stts_) {
            ss << ",";
            ss << "\"stts\":" << stts_->Dump();
        }

        if (ctts_) {
            ss << ",";
            ss << "\"ctts\":"  << ctts_->Dump();
        }
        if (stss_) {
            ss << ",";
            ss << "\"stss\":"  << stss_->Dump();
        }
        if (stsc_) {
            ss << ",";
            ss << "\"stsc\":"  << stsc_->Dump();
        }
        if (stsz_) {
            ss << ",";
            ss << "\"stsz\":"  << stsz_->Dump();
        }
        if (stco_) {
            ss << ",";
            ss << "\"stco\":"  << stco_->Dump();
        }
        if (unknown_boxes_.size() > 0) {
            ss << ",";
            size_t index = 0;
            for (Mp4BoxBase* box : unknown_boxes_) {
                ss << box->Dump();
                index++;
                if (index < unknown_boxes_.size()) {
                    ss << ",";
                }
            }
        }
        ss << "}";
        return ss.str();
    }
public:
    StsdBox* stsd_ = nullptr;
    SttsBox* stts_ = nullptr;
    CttsBox* ctts_ = nullptr;
    StssBox* stss_ = nullptr;
    StscBox* stsc_ = nullptr;
    StszBox* stsz_ = nullptr;
    StcoBox* stco_ = nullptr;
    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

//minf is in mdia. minf has smhd, dinf and stbl
class MinfBox : public Mp4BoxBase
{
public:
    MinfBox() {
        type_ = "minf";
    }
    ~MinfBox() {
        if (smhd_) {
            delete smhd_;
            smhd_ = nullptr;
        }
        if (vmhd_) {
            delete vmhd_;
            vmhd_ = nullptr;
        }
        if (dinf_) {
            delete dinf_;
            dinf_ = nullptr;
        }
        if (stbl_) {
            delete stbl_;
            stbl_ = nullptr;
        }
        for (Mp4BoxBase* box : unknown_boxes_) {
            delete box;
        }
        unknown_boxes_.clear();
    }
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (vmhd_) {
            size_t box_size = 0;
            uint8_t* box_data = vmhd_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (smhd_) {
            size_t box_size = 0;
            uint8_t* box_data = smhd_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (dinf_) {
            size_t box_size = 0;
            uint8_t* box_data = dinf_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (stbl_) {
            size_t box_size = 0;
            uint8_t* box_data = stbl_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while (p < (start + box_size_ - 6)) {
            (void)GetBoxHeaderInfo(p, box_type, offset);
            if (box_type == "smhd") {
                smhd_ = new SmhdBox();
                p = smhd_->Parse(p);
            } else if (box_type == "vmhd") {
                vmhd_ = new VmhdBox();
                p = vmhd_->Parse(p);
            } else if (box_type == "dinf") {
                dinf_ = new DinfBox();
                p = dinf_->Parse(p);
            } else if (box_type == "stbl") {
                stbl_ = new StblBox();
                p = stbl_->Parse(p, mov);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                p += box->box_size_;
                unknown_boxes_.push_back(box);
            }
        }
        assert(p == (start + box_size_));
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_;
        if (smhd_) {
            ss << ",";
            ss << "\"smhd\":" << smhd_->Dump();
        }
        if (vmhd_) {
            ss << ",";
            ss << "\"vmhd\":" << vmhd_->Dump();
        }
        if (dinf_) {
            ss << ",";
            ss << "\"dinf\":" << dinf_->Dump();
        }
        if (stbl_) {
            ss << ",";
            ss << "\"stbl\":" << stbl_->Dump();
        }
        if (unknown_boxes_.size() > 0) {
            ss << ",";
            size_t index = 0;
            for (Mp4BoxBase* box : unknown_boxes_) {
                ss << box->Dump();
                index++;
                if (index < unknown_boxes_.size()) {
                    ss << ",";
                }
            }
        }
        ss << "}";
        return ss.str();
    }
public:
    SmhdBox* smhd_ = nullptr;//for audio
    VmhdBox* vmhd_ = nullptr;//for video
    DinfBox* dinf_ = nullptr;
    StblBox* stbl_ = nullptr;

    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

//mdia is in trak
class MdiaBox : public Mp4BoxBase
{
public:
    MdiaBox() { type_ = "mdia"; }
    ~MdiaBox() {
        if (mdhd_) {
            delete mdhd_;
            mdhd_ = nullptr;
        }
        if (hdlr_) {
            delete hdlr_;
            hdlr_ = nullptr;
        }
        if (minf_) {
            delete minf_;
            minf_ = nullptr;
        }
        for (Mp4BoxBase* box : unknown_boxes_) {
            delete box;
        }
        unknown_boxes_.clear();
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (mdhd_) {
            size_t box_size = 0;
            uint8_t* box_data = mdhd_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (hdlr_) {
            size_t box_size = 0;
            uint8_t* box_data = hdlr_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }
        if (minf_) {
            size_t box_size = 0;
            uint8_t* box_data = minf_->Serialize(box_size);
            memcpy(p, box_data, box_size);
            p += box_size;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while (p < (start + box_size_ - 6)) {
            (void)GetBoxHeaderInfo(p, box_type, offset);
            if (box_type == "mdhd") {
                mdhd_ = new MdhdBox();
                p = mdhd_->Parse(p, mov);
            } else if (box_type == "hdlr") {
                hdlr_ = new HdlrBox();
                p = hdlr_->Parse(p, mov);
            } else if (box_type == "minf") {
                minf_ = new MinfBox();
                p = minf_->Parse(p, mov);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                unknown_boxes_.push_back(box);
                p += box->box_size_;
            }
        }
        assert(p == (start + box_size_));

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_;

        if (mdhd_) {
            ss << ",";
            ss << "\"mdhd\":" << mdhd_->Dump();
        }
        if (hdlr_) {
            ss << ",";
            ss << "\"hdlr\":" << hdlr_->Dump();
        }
        if (minf_) {
            ss << ",";
            ss << "\"minf\":" << minf_->Dump();
        }
        if (unknown_boxes_.size() > 0) {
            ss << ",";
            size_t index = 0;
            for (Mp4BoxBase* box : unknown_boxes_) {
                ss << box->Dump();
                index++;
                if (index < unknown_boxes_.size()) {
                    ss << ",";
                }
            }
        }
        ss << "}";

        return ss.str();
    }
public:
    MdhdBox* mdhd_ = nullptr;
    HdlrBox* hdlr_ = nullptr;
    MinfBox* minf_ = nullptr;

    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

//elst is in edts
class ElstBox : public Mp4BoxBase
{
public:
    typedef struct ElstInfo_S {
        uint64_t segment_duration_ = 0;//if version == 0, it's 32bits; if version == 1, it's 64bits
        uint64_t media_time_ = 0;//if version == 0, it's 32bits; if version == 1, it's 64bits
        uint16_t media_rate_integer_ = 0;
        uint16_t media_rate_fraction_ = 0;
    } ElstInfo;

public:
    ElstBox() { type_ = "elst"; }
    ~ElstBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;
        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, entry_count_);
        p += 4;

        uint8_t ver = (version_flag_ >> 24) & 0xff;
        for (size_t i = 0; i < entry_count_; i++) {
            if (ver == 0) {
                ByteStream::Write4Bytes(p, elst_list_[i].segment_duration_);
                p += 4;
                ByteStream::Write4Bytes(p, elst_list_[i].media_time_);
                p += 4;
            } else {
                ByteStream::Write8Bytes(p, elst_list_[i].segment_duration_);
                p += 8;
                ByteStream::Write8Bytes(p, elst_list_[i].media_time_);
                p += 8;
            }
            ByteStream::Write2Bytes(p, elst_list_[i].media_rate_integer_);
            p += 2;
            ByteStream::Write2Bytes(p, elst_list_[i].media_rate_fraction_);
            p += 2;
        }

        len = (int)(p - data);
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        entry_count_ = ByteStream::Read4Bytes(p);
        p += 4;
        
        uint8_t ver = (version_flag_ >> 24) & 0xff;
        for (size_t i = 0; i < entry_count_; i++) {
            ElstInfo info;
            if (ver == 0) {
                info.segment_duration_ = ByteStream::Read4Bytes(p);
                p += 4;
                info.media_time_ = ByteStream::Read4Bytes(p);
                p += 4;
            } else {
                info.segment_duration_ = ByteStream::Read8Bytes(p);
                p += 8;
                info.media_time_ = ByteStream::Read8Bytes(p);
                p += 8;
            }
            info.media_rate_integer_ = ByteStream::Read2Bytes(p);
            p += 2;
            info.media_rate_fraction_ = ByteStream::Read2Bytes(p);
            p += 2;

            elst_list_.push_back(info);
        }
        assert(p == start + box_size_);
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (int)(version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"entry_count_\":" << entry_count_ << (entry_count_ > 0 ? "," : "");
        ss << "\"entries\":[";
        for (size_t i = 0; i < entry_count_; i++) {
            if (i >= elst_list_.size()) {
                break;
            }
            ss << "{";
            ss << "\"segment_duration\":" << elst_list_[i].segment_duration_ << ",";
            ss << "\"media_time\":" << elst_list_[i].media_time_ << ",";
            ss << "\"media_rate_integer\":" << elst_list_[i].media_rate_integer_ << ",";
            ss << "\"media_rate_fraction\":" << elst_list_[i].media_rate_fraction_;
            ss << "}";
            if (i < entry_count_ - 1) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";

        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;

    std::vector<ElstInfo> elst_list_;

private:
    std::vector<uint8_t> serialize_data_;
};

//edts is in trak
class EdtsBox : public Mp4BoxBase
{
public:
    EdtsBox() { type_ = "edts"; }
    ~EdtsBox() {
        if (elst_) {
            delete elst_;
            elst_ = nullptr;
        }
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 4 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        size_t elst_len = 0;
        uint8_t* elst_data = elst_->Serialize(elst_len);
        memcpy(p, elst_data, elst_len);
        p += elst_len;

        len = (size_t)(p - data);
        box_size_ = (uint64_t)(p - data);
        ByteStream::Write4Bytes(data, (uint32_t)(box_size_));
        return data;
    }

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        if (elst_) {
            delete elst_;
        }
        elst_ = new ElstBox();
        p = elst_->Parse(p);

        assert(p == (start + box_size_));

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << (elst_ != nullptr ? "," : "");

        if (elst_) {
            ss << "\"elst\":" << elst_->Dump();
        }
        
        ss << "}";

        return ss.str();
    }
public:
    ElstBox* elst_ = nullptr;
private:
    std::vector<uint8_t> serialize_data_;
};

//meta is in udta
class MetaBox : public Mp4BoxBase
{
public:
    MetaBox() { type_ = "udta"; }
    ~MetaBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        size_t data_size = (start + box_size_) - p;

        data_.resize(data_size);
        uint8_t* data = (uint8_t*)&data_[0];
        memcpy(data, p, data_size);

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"data\":\"";

        uint8_t* p = (uint8_t*)&(data_[0]);
        ss << Data2HexString(p, data_.size());
        ss << "\"";
        ss << "}";

        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    std::vector<uint8_t> data_;
};

//udta is in moov
class UdtaBox : public Mp4BoxBase
{
public:
    UdtaBox() { type_ = "udta"; }
    ~UdtaBox() {
        if (meta_) {
            delete meta_;
            meta_ = nullptr;
        }
    }

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        if (p < start + box_size_) {
            meta_ = new MetaBox();
            p = meta_->Parse(p);
        }
        assert(p == (start + box_size_));
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << (meta_ ? "," : "");
        if (meta_) {
            ss << "\"meta\":" << meta_->Dump();
        }
        ss << "}";

        return ss.str();
    }

public:
    MetaBox* meta_ = nullptr;
};

//mdat is a root
class MdatBox : public Mp4BoxBase
{
public:
    MdatBox() { type_ = "mdat"; }
    ~MdatBox() {}

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        (void)Mp4BoxBase::Parse(start);

        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << "";
        ss << "}";

        return ss.str();
    }
};

//trak is in moov
class TrakBox : public Mp4BoxBase
{
public:
    TrakBox() { type_ = "trak"; }
    ~TrakBox() {
        if (tkhd_) {
            delete tkhd_;
            tkhd_ = nullptr;
        }
        if (edts_) {
            delete edts_;
            edts_ = nullptr;
        }
        if (mdia_) {
            delete mdia_;
            mdia_ = nullptr;
        }
        for (Mp4BoxBase* box : unknown_boxes_) {
            delete box;
        }
        unknown_boxes_.clear();
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 300*1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        memset(data, 0, default_size);

        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (tkhd_) {
            size_t tkhd_size = 0;
            uint8_t* tkhd_data = tkhd_->Serialize(tkhd_size);
            memcpy(p, tkhd_data, tkhd_size);
            p += tkhd_size; 
        }
        if (edts_) {
            size_t edts_size = 0;
            uint8_t* edts_data = edts_->Serialize(edts_size);
            memcpy(p, edts_data, edts_size);
            p += edts_size;
        }
        if (mdia_) {
            size_t mdia_size = 0;
            uint8_t* mdia_data = mdia_->Serialize(mdia_size);
            memcpy(p, mdia_data, mdia_size);
            p += mdia_size;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);

        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        
        while(p < start + box_size_) {
            std::string box_type;
            int offset = 0;

            GetBoxHeaderInfo(p, box_type, offset);
            if (box_type == "tkhd") {
                tkhd_ = new TkhdBox();
                p = tkhd_->Parse(p, mov);
            } else if (box_type == "edts") {
                edts_ = new EdtsBox();
                p = edts_->Parse(p);
            } else if (box_type == "mdia") {
                mdia_ = new MdiaBox();
                p = mdia_->Parse(p, mov);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                p += box->box_size_;
                unknown_boxes_.push_back(box);
            }
        }

        return start + box_size_;
    }
    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_;
        if (tkhd_) {
            ss << ",";
            ss << "\"tkhd\":" << tkhd_->Dump();
        }
        if (edts_) {
            ss << ",";
            ss << "\"edts\":" << edts_->Dump();
        }
        if (mdia_) {
            ss << ",";
            ss << "\"mdia\":" << mdia_->Dump();
        }
        if (unknown_boxes_.size() > 0) {
            ss << ",";
            size_t index = 0;
            for (Mp4BoxBase* box : unknown_boxes_) {
                ss << box->Dump();
                index++;
                if (index < unknown_boxes_.size()) {
                    ss << ",";
                }
            }
        }
        ss << "}";

        return ss.str();
    }

public:
    TkhdBox* tkhd_ = nullptr;
    EdtsBox* edts_ = nullptr;
    MdiaBox* mdia_ = nullptr;

    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

//moov is a root box
class MoovBox : public Mp4BoxBase
{
public:
    MoovBox() { type_ = "moov"; }
    ~MoovBox() {
        if (mvhd_) {
            delete mvhd_;
            mvhd_ = nullptr;
        }
        if (udta_) {
            delete udta_;
            udta_ = nullptr;
        }
        for (TrakBox* trak_box : traks_) {
            delete trak_box;
        }
        traks_.clear();

        for (Mp4BoxBase* box : unknown_boxes_) {
            delete box;
        }
        unknown_boxes_.clear();
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 10*1024;
        uint8_t* data = new uint8_t[default_size];
        uint8_t* p = data;

        memset(data, 0, default_size);

        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        size_t mvhd_size = 0;
        uint8_t* mvhd_data = mvhd_->Serialize(mvhd_size);
        memcpy(p, mvhd_data, mvhd_size);
        p += mvhd_size;

        for (size_t i = 0; i < traks_.size(); i++) {
            size_t trak_size = 0;
            uint8_t* trak_data = traks_[i]->Serialize(trak_size);
            memcpy(p, trak_data, trak_size);
            p += trak_size;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);
        return data;
    }

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while (p < start + box_size_) {
            GetBoxHeaderInfo(p, box_type, offset);
            
            if (box_type == "mvhd") {
                mvhd_ = new MvhdBox();
                p = mvhd_->Parse(p, mov);
            } else if (box_type == "trak") {
                TrakBox* trak = new TrakBox();

                mov.traks_info_.resize(mov.traks_info_.size() + 1);
                p = trak->Parse(p, mov);
                traks_.push_back(trak);
            } else if (box_type == "udta") {
                udta_ = new UdtaBox();
                p = udta_->Parse(p);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                p += box->box_size_;
                unknown_boxes_.push_back(box);
            }
        }

        //p = p + offset_;
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ((mvhd_ != nullptr || !traks_.empty() || udta_ != nullptr) ? ",": "");
        if (mvhd_) {
            ss << "\"mvhd\":" << mvhd_->Dump();
            ss << ((!traks_.empty() || udta_ != nullptr) ? ",": "");
        }

        int i = 0;
        for (TrakBox* track : traks_) {
            ss << "\"track" << i << "\":" << track->Dump();
            ss << (((i != traks_.size() - 1) || udta_ != nullptr) ? ",": "");
            i++;
        }
        if (udta_) {
            ss << "\"udta\":" << udta_->Dump();
        }
        if (unknown_boxes_.size() > 0) {
            ss << ",";
            size_t index = 0;
            for (Mp4BoxBase* box : unknown_boxes_) {
                ss << "\"" << box->type_ << "\":" << box->Dump();
                index++;
                if (index < unknown_boxes_.size()) {
                    ss << ",";
                }
            }
        }
        ss << "}";
        
        return ss.str();
    }
public:
    MvhdBox* mvhd_ = nullptr;
    std::vector<TrakBox*> traks_;
    UdtaBox* udta_ = nullptr;

    std::vector<Mp4BoxBase*> unknown_boxes_;
};

//free is a root box
class FreeBox : public Mp4BoxBase
{
public:
    FreeBox() { type_ = "free"; }
    ~FreeBox() {}

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << "";
        ss << "}";

        return ss.str();
    }
};

// mfhd box is a container for movie fragments in moof box
class MfhdBox : public Mp4BoxBase
{
public:
    MfhdBox() { type_ = "mfhd"; }
    ~MfhdBox() {}

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        // Parse the mfhd box data
        version_flag_ = ByteStream::Read4Bytes(p); // version and flags
        p += 4;
        sequence_number_ = ByteStream::Read4Bytes(p); // sequence number
        p += 4;
        
        assert(p == (start + box_size_));
        return p;
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 64;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        ByteStream::Write4Bytes(p, 0);//reserve size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, sequence_number_);
        p += 4;

        len = (size_t)(p - data);
        box_size_ = (uint64_t)(p - data);
        ByteStream::Write4Bytes(data, (uint32_t)(box_size_));
        return data;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << "";
        ss << "}";

        return ss.str();
    }

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t sequence_number_ = 0;

private:
    std::vector<uint8_t> serialize_data_;
};

// tfhd box is a container in traf box: 
class TfhdBox : public Mp4BoxBase
{
public:
    TfhdBox() { type_ = "tfhd"; }
    ~TfhdBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 64;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        memset(data, 0, default_size);

        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, track_id_);
        p += 4;

        uint32_t flag = version_flag_ & 0xFFFFFF;
        if (flag & 0x000001) {
            ByteStream::Write8Bytes(p, base_data_offset_);
            p += 8;
        }
        if (flag & 0x000002) {
            ByteStream::Write4Bytes(p, sample_description_index_);
            p += 4;
        }
        if (flag & 0x000008) {
            ByteStream::Write4Bytes(p, default_sample_duration_);
            p += 4;
        }
        if (flag & 0x000010) {
            ByteStream::Write4Bytes(p, default_sample_size_);
            p += 4;
        }
        if (flag & 0x000020) {
            ByteStream::Write4Bytes(p, default_sample_flags_);
            p += 4;
        }

        len = (size_t)(p - data);
        box_size_ = (uint64_t)(p - data);
        ByteStream::Write4Bytes(data, (uint32_t)(box_size_));
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        track_id_ = ByteStream::Read4Bytes(p);
        p += 4;

        uint32_t flag = version_flag_ & 0xFFFFFF;
        if (flag & 0x000001) {
            base_data_offset_ = ByteStream::Read8Bytes(p);
            p += 8;
        }
        if (flag & 0x000002) {
            sample_description_index_ = ByteStream::Read4Bytes(p);
            p += 4;
        }
        if (flag & 0x000008) {
            default_sample_duration_ = ByteStream::Read4Bytes(p);
            p += 4;
        }
        if (flag & 0x000010) {
            default_sample_size_ = ByteStream::Read4Bytes(p);
            p += 4;
        }
        if (flag & 0x000020) {
            default_sample_flags_ = ByteStream::Read4Bytes(p);
            p += 4;
        }

        assert(p == (start + box_size_));
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (int)(version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"track_id\":" << track_id_ << ",";

        uint32_t flag = version_flag_ & 0xFFFFFF;
        if (flag & 0x000001) {
            ss << "\"base_data_offset\":" << base_data_offset_ << ",";
        }
        if (flag & 0x000002) {
            ss << "\"sample_description_index\":" << sample_description_index_ << ",";
        }
        if (flag & 0x000008) {
            ss << "\"default_sample_duration\":" << default_sample_duration_ << ",";
        }
        if (flag & 0x000010) {
            ss << "\"default_sample_size\":" << default_sample_size_ << ",";
        }
        if (flag & 0x000020) {
            ss << "\"default_sample_flags\":" << default_sample_flags_ << ",";
        }
        std::string str = ss.str();
        if (str.back() == ',') {
            str.pop_back();
        }
        str += "}";
        return str;
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t track_id_ = 0;
public:
    uint64_t base_data_offset_ = 0;
    uint32_t sample_description_index_ = 0;
    uint32_t default_sample_duration_ = 0;
    uint32_t default_sample_size_ = 0;
    uint32_t default_sample_flags_ = 0;

private:
    std::vector<uint8_t> serialize_data_;
};

class TfdtBox : public Mp4BoxBase
{
public:
    TfdtBox() { type_ = "tfdt"; }
    ~TfdtBox() {}

uint8_t* Serialize(size_t& len) {
        const size_t default_size = 32;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        memset(data, 0, default_size);

        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;

        uint8_t ver = (version_flag_ >> 24) & 0xff;
        if (ver == 0) {
            ByteStream::Write4Bytes(p, (uint32_t)base_media_decode_time_);
            p += 4;
        } else {
            ByteStream::Write8Bytes(p, base_media_decode_time_);
            p += 8;
        }

        len = (size_t)(p - data);
        box_size_ = (uint64_t)(p - data);
        ByteStream::Write4Bytes(data, (uint32_t)(box_size_));
        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;

        uint8_t ver = (version_flag_ >> 24) & 0xff;
        if (ver == 0) {
            base_media_decode_time_ = ByteStream::Read4Bytes(p);
            p += 4;
        } else {
            base_media_decode_time_ = ByteStream::Read8Bytes(p);
            p += 8;
        }

        assert(p == (start + box_size_));
        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (int)(version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"base_media_decode_time\":" << base_media_decode_time_;
        ss << "}";

        return ss.str();
    }

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint64_t base_media_decode_time_ = 0;

private:
    std::vector<uint8_t> serialize_data_;
};

class TrunBox : public Mp4BoxBase
{
public:
    TrunBox() { type_ = "trun"; }
    ~TrunBox() {}

public:
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        sample_count_ = ByteStream::Read4Bytes(p);
        p += 4;

        uint32_t flag = version_flag_ & 0xFFFFFF;
        if (flag & 0x000001) {
            data_offset_ = ByteStream::Read4Bytes(p);
            p += 4;
        }
        if (flag & 0x000004) {
            first_sample_flags_ = ByteStream::Read4Bytes(p);
            p += 4;
        }

        for (size_t i = 0; i < sample_count_; i++) {
            if (flag & 0x000100) {
                uint32_t sample_duration = ByteStream::Read4Bytes(p);
                p += 4;
                sample_durations_.push_back(sample_duration);
            }
            if (flag & 0x000200) {
                uint32_t sample_size = ByteStream::Read4Bytes(p);
                p += 4;
                sample_sizes_.push_back(sample_size);
            }
            if (flag & 0x000400) {
                uint32_t sample_flags = ByteStream::Read4Bytes(p);
                p += 4;
                sample_flags_.push_back(sample_flags);
            }
            if (flag & 0x000800) {
                uint32_t sample_composition_time_offset = ByteStream::Read4Bytes(p);
                p += 4;
                sample_composition_time_offset_.push_back(sample_composition_time_offset);
            }
        }

        assert(p == (start + box_size_));
        return start + box_size_;
    }

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 200 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        memset(data, 0, default_size);

        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        ByteStream::Write4Bytes(p, version_flag_);
        p += 4;
        ByteStream::Write4Bytes(p, sample_count_);
        p += 4;

        uint32_t flag = version_flag_ & 0xFFFFFF;
        if (flag & 0x000001) {
            ByteStream::Write4Bytes(p, data_offset_);
            p += 4;
        }
        if (flag & 0x000004) {
            ByteStream::Write4Bytes(p, first_sample_flags_);
            p += 4;
        }

        for (size_t i = 0; i < sample_count_; i++) {
            if (flag & 0x000100) {
                uint32_t sample_duration = 0;
                if (i < sample_durations_.size()) {
                    sample_duration = sample_durations_[i];
                }
                ByteStream::Write4Bytes(p, sample_duration);
                p += 4;
            }
            if (flag & 0x000200) {
                uint32_t sample_size = 0;
                if (i < sample_sizes_.size()) {
                    sample_size = sample_sizes_[i];
                }
                ByteStream::Write4Bytes(p, sample_size);
                p += 4;
            }
            if (flag & 0x000400) {
                uint32_t sample_flags = 0;
                if (i < sample_flags_.size()) {
                    sample_flags = sample_flags_[i];
                }
                ByteStream::Write4Bytes(p, sample_flags);
                p += 4;
            }
            if (flag & 0x000800) {
                uint32_t sample_composition_time_offset = 0;
                if (i < sample_composition_time_offset_.size()) {
                    sample_composition_time_offset = sample_composition_time_offset_[i];
                }
                ByteStream::Write4Bytes(p, sample_composition_time_offset);
                p += 4;
            }
        }

        return data;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"version\":" << (int)(version_flag_ >> 24) << ",";
        ss << "\"flag\":" << (version_flag_ & 0xffffff) << ",";
        ss << "\"sample_count\":" << sample_count_ << ",";
        ss << "\"data_offset\":" << data_offset_ << ",";
        ss << "\"first_sample_flags\":" << first_sample_flags_ << ",";

        ss << "\"samples\":[";
        for (size_t i = 0; i < sample_count_; i++) {
            ss << "{";
            if (i < sample_durations_.size()) {
                ss << "\"sample_duration\":" << sample_durations_[i] << ",";
            }
            if (i < sample_sizes_.size()) {
                ss << "\"sample_size\":" << sample_sizes_[i] << ",";
            }
            if (i < sample_flags_.size()) {
                ss << "\"sample_flags\":" << sample_flags_[i] << ",";
            }
            if (i < sample_composition_time_offset_.size()) {
                ss << "\"sample_composition_time_offset\":" << sample_composition_time_offset_[i] << ",";
            }
            std::string str = ss.str();
            if (str.back() == ',') {
                str.pop_back();
            }
            str += "}";
            ss.str("");
            ss.clear();
            ss << str;
            if (i < sample_count_ - 1) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";

        return ss.str();
    }

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t sample_count_ = 0;
    uint32_t data_offset_ = 0;
    uint32_t first_sample_flags_ = 0;
    std::vector<uint32_t> sample_durations_;
    std::vector<uint32_t> sample_sizes_;
    std::vector<uint32_t> sample_flags_;
    std::vector<uint32_t> sample_composition_time_offset_;

private:
    std::vector<uint8_t> serialize_data_;
};

// traf box is a container for track fragments in moof box
class TrafBox : public Mp4BoxBase
{
public:
    TrafBox() { type_ = "traf"; }
    ~TrafBox() {}

    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 200 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        memset(data, 0, default_size);

        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;
        
        if (tfhd_) {
            size_t tfhd_size = 0;
            uint8_t* tfhd_data = tfhd_->Serialize(tfhd_size);
            memcpy(p, tfhd_data, tfhd_size);
            p += tfhd_size;
        }
        if (tfdt_) {
            size_t tfdt_size = 0;
            uint8_t* tfdt_data = tfdt_->Serialize(tfdt_size);
            memcpy(p, tfdt_data, tfdt_size);
            p += tfdt_size;
        }
        if (trun_) {
            size_t trun_size = 0;
            uint8_t* trun_data = trun_->Serialize(trun_size);
            memcpy(p, trun_data, trun_size);
            p += trun_size;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);

        return data;
    }
    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while (p < start + box_size_) {
            GetBoxHeaderInfo(p, box_type, offset);
            if (box_type == "tfhd") {
                tfhd_ = new TfhdBox();
                p = tfhd_->Parse(p, mov);
            } else if (box_type == "tfdt") {
                tfdt_ = new TfdtBox();
                p = tfdt_->Parse(p, mov);
            } else if (box_type == "trun") {
                trun_ = new TrunBox();
                p = trun_->Parse(p, mov);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                unknown_boxes_.push_back(box);
                p += box->box_size_;
            }
        }

        return p;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << "";
        if (tfhd_) {
            ss << ",\"tfhd\":" << tfhd_->Dump();
        }
        if (tfdt_) {
            ss << ",\"tfdt\":" << tfdt_->Dump();
        }
        if (trun_) {
            ss << ",\"trun\":" << trun_->Dump();
        }
        ss << "}";

        return ss.str();
    }

public:
    TfhdBox* tfhd_ = nullptr;
    TfdtBox* tfdt_ = nullptr;
    TrunBox* trun_ = nullptr;

    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

//moof box is a container for media data fragments
class MoofBox : public Mp4BoxBase
{
public:
    MoofBox() { type_ = "moof"; }
    ~MoofBox() {}

    uint8_t* Parse(uint8_t* start, MovInfo& mov) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while (p < start + box_size_) {
            GetBoxHeaderInfo(p, box_type, offset);
            if (box_type == "mfhd") {
                mfhd_ = new MfhdBox();
                p = mfhd_->Parse(p, mov);
            } else if (box_type == "traf") {
                TrafBox* traf = new TrafBox();
                p = traf->Parse(p, mov);
                traks_.push_back(traf);
            } else {
                Mp4BoxBase* box = new Mp4BoxBase();
                box->Parse(p);
                unknown_boxes_.push_back(box);
                p += box->box_size_;
            }
        }
        assert(p == start + box_size_);
        return start + box_size_;
    }
    uint8_t* Serialize(size_t& len) {
        const size_t default_size = 100 * 1024;
        serialize_data_.resize(default_size);
        uint8_t* data = &serialize_data_[0];
        uint8_t* p = data;

        memset(data, 0, default_size);

        ByteStream::Write4Bytes(p, 0); //placeholder for size
        p += 4;
        ByteStream::Write4Bytes(p, MAKE_TAG(type_[0], type_[1], type_[2], type_[3]));
        p += 4;

        if (mfhd_) {
            size_t mfhd_size = 0;
            uint8_t* mfhd_data = mfhd_->Serialize(mfhd_size);
            memcpy(p, mfhd_data, mfhd_size);
            p += mfhd_size;
        }

        for (size_t i = 0; i < traks_.size(); i++) {
            size_t traf_size = 0;
            uint8_t* traf_data = traks_[i]->Serialize(traf_size);
            memcpy(p, traf_data, traf_size);
            p += traf_size;
        }

        len = p - data;
        box_size_ = len;
        ByteStream::Write4Bytes(data, (uint32_t)len);

        return data;
    }
    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << "";
        if (mfhd_) {
            ss << ",\"mfhd\":" << mfhd_->Dump();
        }
        for (size_t i = 0; i < traks_.size(); i++) {
            ss << ",\"traf_" << i << "\":" << traks_[i]->Dump();
        }
        for (size_t i = 0; i < unknown_boxes_.size(); i++) {
            ss << ",\"unknown_" << i << "\":" << unknown_boxes_[i]->Dump();
        }
        ss << "}";

        return ss.str();
    }

public:
    MfhdBox* mfhd_ = nullptr;
    std::vector<TrafBox*> traks_;
    
    std::vector<Mp4BoxBase*> unknown_boxes_;

private:
    std::vector<uint8_t> serialize_data_;
};

}

#endif//MP4_BOX_HPP