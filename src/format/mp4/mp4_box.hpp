#ifndef MP4_BOX_HPP
#define MP4_BOX_HPP
#include "byte_stream.hpp"
#include "stringex.hpp"

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

typedef enum ENUM_MOV_MEDIA_TYPE_S {
    UNKNOWN_BOX_TYPE,
    VIDEO_BOX_TYPE,
    AUDIO_BOX_TYPE,
    SUBTITLE_BOX_TYPE
} ENUM_MOV_MEDIA_TYPE;

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
public:
    uint64_t box_size_ = 0;
    std::string type_;
    uint8_t* start_ = nullptr;
    int offset_ = 0;

    Mp4BoxBase* father_ = nullptr;
    std::vector<Mp4BoxBase*> childrens_;
};

#define BRANDS_MAX 8

//ftyp is a root box
class FtypBox : public Mp4BoxBase
{
public:
    FtypBox() { type_ = "ftyp"; }
    ~FtypBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        major_brand_ = ByteStream::Read4Bytes(p);
        p += 4;
        minor_version_ = ByteStream::Read4Bytes(p);
        p += 4;

        int i = 0;
        while (p < (start + box_size_)) {
            brands_count_++;
            compatible_brands_[i++] = ByteStream::Read4Bytes(p);
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
};

//mvhd is in moov
class MvhdBox : public Mp4BoxBase
{
public:
    MvhdBox() { type_ = "mvhd"; }
    ~MvhdBox() {}

    uint8_t* Parse(uint8_t* start) {
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
    uint32_t reserve2_[2];
    uint32_t matrix_[9]; //65536,0,0,0,65536,0,0,0,1073741824
    uint32_t pre_defined_[6];
    uint32_t next_track_id_;
};

//tkhd is in trak
class TkhdBox : public Mp4BoxBase
{
public:
    TkhdBox() { type_ = "tkhd"; }
    ~TkhdBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        uint8_t ver = (uint8_t)(version_flag_ >> 24);
        p += 4;

        creation_time_ = (ver == 0) ? ByteStream::Read4Bytes(p) : ByteStream::Read8Bytes(p);
        p += (ver == 0) ? 4 : 8;

        modification_time_ = (ver == 0) ? ByteStream::Read4Bytes(p) : ByteStream::Read8Bytes(p);
        p += (ver == 0) ? 4 : 8;

        track_id_ = ByteStream::Read4Bytes(p);
        p += 4;

        reserved1_ = ByteStream::Read4Bytes(p);
        p += 4;

        duration_ = (ver == 0) ? ByteStream::Read4Bytes(p) : ByteStream::Read8Bytes(p);
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
        p += 4;

        height_ = ByteStream::Read4Bytes(p);
        p += 4;

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

    uint32_t transform_matrix_[9];
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

//mdhd is in mdia
class MdhdBox : public Mp4BoxBase
{
public:
    MdhdBox() { type_ = "mdhd"; }
    ~MdhdBox() {}

public:
    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        
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
};

//hdlr is in mdia
class HdlrBox : public Mp4BoxBase
{
public:
    HdlrBox() { type_ = "hdlr"; }
    ~HdlrBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

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

        std::string type_str = Uint32ToString(handler_type_);
        if (type_str == "soun") {
            media_type_ = AUDIO_BOX_TYPE;
        } else if (type_str == "vide") {
            media_type_ = VIDEO_BOX_TYPE;
        }
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

    ENUM_MOV_MEDIA_TYPE media_type_ = UNKNOWN_BOX_TYPE;
};

//smhd is in minf
class SmhdBox : public Mp4BoxBase
{
public:
    SmhdBox() { type_ = "smhd"; }
    ~SmhdBox() {}

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
};

class VmhdBox : public Mp4BoxBase
{
public:
    VmhdBox() { type_ = "vmhd"; }
    ~VmhdBox() {}

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
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint8_t graphicsmode_ = 0;
    uint8_t opcolor_[3];
};

//url is in DrefBox
class UrlBox : public Mp4BoxBase
{
public:
    UrlBox() { type_ = "url "; }
    ~UrlBox() {}

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
    std::string location_;
};

//dref is in dinf
class DrefBox : public Mp4BoxBase
{
public:
    DrefBox() { type_ = "dref"; }
    ~DrefBox() {}

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
};

//dinf is in minf, it has dref
class DinfBox : public Mp4BoxBase
{
public:
    DinfBox() { type_ = "dinf"; }
    ~DinfBox() {}

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
};

//next box: avcC(h264), hvcC(h265), av1C(av1), vvcC(h266), vpcC(vp8, vp9)
class VideoSequencBox : public Mp4BoxBase
{
public:
    VideoSequencBox() {}
    ~VideoSequencBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        size_t data_len = box_size_ - 8;

        data_.resize(data_len);
        uint8_t* header = (uint8_t*)&(data_[0]);

        memcpy(header, p, data_len);

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
        ss << "\"data\":[";
        for (size_t i = 0; i < data_.size(); i++) {
            ss << (int)data_[i];
            if (i != data_.size() - 1) {
                ss << ",";
            }
        }
        ss << "]";
        ss << "}";
        return ss.str();
    }
public:
    std::vector<uint8_t> data_;
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

class Avc1Box : public Mp4BoxBase
{
public:
    Avc1Box() {
        type_ = "avc1";
        memset(compressorname_, 0, sizeof(compressorname_));
    }
    ~Avc1Box() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

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
        p += 2;
        height_ = ByteStream::Read2Bytes(p);
        p += 2;

        horizontal_resolution_ = ByteStream::Read4Bytes(p);
        p += 4;
        vertical_resolution_ = ByteStream::Read4Bytes(p);
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

        //next box: avcC(h264), hvcC(h265), av1C(av1), vvcC(h266), vpcC(vp8, vp9)
        video_hdr_box_ = new VideoSequencBox();
        p = video_hdr_box_->Parse(p);

        std::cout << "avc1 p:" << (uint64_t)p << ", start:" << (uint64_t)start << ", box_size:" << box_size_ << "\r\n\r\n";

        if(p == start + box_size_) {
            return start + box_size_;
        }

        std::string box_type;
        int offset = 0;
        GetBoxHeaderInfo(p, box_type, offset);
        if (box_type == "pasp") {
            pasp_box_ = new PaspBox();
            p = pasp_box_->Parse(p);
        }
        assert(p == start + box_size_);

        return start + box_size_;
    }

    std::string Dump() {
        std::stringstream ss;

        ss << "{";
        ss << "\"type\":\"" << type_ << "\",";
        ss << "\"size\":" << box_size_ << ",";
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
        ss << "\"frame_count_\":" << frame_count_ << ",";
        ss << "\"compressorname\":[";
        for (size_t i = 0; i < sizeof(compressorname_); i++) {
            ss << (int)compressorname_[i];
            if (i != sizeof(compressorname_) - 1) {
                ss << ",";
            }
        }
        ss << "],";
        ss << "\"alpha\":" << alpha_ << ",";
        ss << "\"reserved4\":" << reserved4_ << ",";
        ss << "\"video_hdr\":" << video_hdr_box_->Dump();
        if (pasp_box_) {
            ss << ",";
            ss << "\"pasp\":" << pasp_box_->Dump();
        }
        ss << "}";

        return ss.str();
    }

public:
    uint32_t reserved1_ = 0;
    uint16_t reserved2_ = 0;
    uint16_t data_reference_index_ = 0;
    uint16_t codec_stream_version_ = 0;//Reserved
    uint16_t codec_stream_reversion_ = 0;//Reserved
    uint32_t reserved3_[3];
    uint16_t width_ = 0;
    uint16_t height_ = 0;
    uint32_t horizontal_resolution_ = 0;
    uint32_t vertical_resolution_ = 0;
    uint32_t data_size_ = 0;//Reserved
    uint16_t frame_count_ = 0;//frame cout == 1
    char compressorname_[32];//0 for default
    uint16_t alpha_ = 0x18;
    uint16_t reserved4_ = 0xffff;

    VideoSequencBox* video_hdr_box_ = nullptr;
    PaspBox* pasp_box_ = nullptr;
};

//stsd is in stbl, 
class StsdBox : public Mp4BoxBase
{
public:
    StsdBox(ENUM_MOV_MEDIA_TYPE media_type) {
        type_ = "stsd";
        media_type_ = media_type;
    }
    ~StsdBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);

        version_flag_ = ByteStream::Read4Bytes(p);
        p += 4;
        entry_count_ = ByteStream::Read4Bytes(p);
        p += 4;

        std::string box_type;
        int offset = 0;

        GetBoxHeaderInfo(p, box_type, offset);
        if (box_type == "avc1") {
            avc1_box_ = new Avc1Box();
            p = avc1_box_->Parse(p);
        } else if (box_type == "mp4a") {
            p = start + box_size_;
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
        ss << "\"flag\":" << (version_flag_ & 0xffffff);
        std::cout << "media_type:" << media_type_ << ", avc1 box:" << (uint64_t)avc1_box_ << "\r\n\r\n";

        if (media_type_ == VIDEO_BOX_TYPE && avc1_box_ != nullptr) {
            ss << ",";
            ss << "\"avc1\":" << avc1_box_->Dump();
        };
        ss << "}";
        
        return ss.str();
    }

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    Avc1Box* avc1_box_ = nullptr;

    ENUM_MOV_MEDIA_TYPE media_type_ = UNKNOWN_BOX_TYPE;
};

//stbl is in minf. it has stsd, stts, stsc, stsz, stco, sgpd, sbgp
class StblBox : public Mp4BoxBase
{
public:
    StblBox(ENUM_MOV_MEDIA_TYPE media_type) {
        type_ = "stbl";
        media_type_ = media_type;
    }
    ~StblBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while(p < start + box_size_) {
            GetBoxHeaderInfo(p, box_type, offset);
            if (box_type == "stsd") {
                stsd_ = new StsdBox(media_type_);
                p = stsd_->Parse(p);
            } else {
                std::cout << "unknown box type:" << box_type << "\r\n\r\n";
                p = start + box_size_;
                break;
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
        ss << "\"stsd\":" << stsd_->Dump();
        ss << "}";

        std::cout << "stbl box dump:" << stsd_->Dump() << "\r\n\r\n";
        return ss.str();
    }
public:
    StsdBox* stsd_ = nullptr;
    SttsBox* stts_ = nullptr;
    StscBox* stsc_ = nullptr;
    StszBox* stsz_ = nullptr;
    StcoBox* stco_ = nullptr;

    ENUM_MOV_MEDIA_TYPE media_type_;
};

//minf is in mdia. minf has smhd, dinf and stbl
class MinfBox : public Mp4BoxBase
{
public:
    MinfBox(ENUM_MOV_MEDIA_TYPE media_type) {
        type_ = "minf";
        media_type_ = media_type;
    }
    ~MinfBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while (p < (start + box_size_)) {
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
                stbl_ = new StblBox(media_type_);
                p = stbl_->Parse(p);
            } else {
                p = start + box_size_;
                break;
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
        ss << "}";
        return ss.str();
    }
public:
    SmhdBox* smhd_ = nullptr;//for audio
    VmhdBox* vmhd_ = nullptr;//for video
    DinfBox* dinf_ = nullptr;
    StblBox* stbl_ = nullptr;

    ENUM_MOV_MEDIA_TYPE media_type_ = UNKNOWN_BOX_TYPE;
};

//mdia is in trak
class MdiaBox : public Mp4BoxBase
{
public:
    MdiaBox() { type_ = "mdia"; }
    ~MdiaBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while (p < (start + box_size_)) {
            (void)GetBoxHeaderInfo(p, box_type, offset);
            if (box_type == "mdhd") {
                mdhd_ = new MdhdBox();
                p = mdhd_->Parse(p);
            } else if (box_type == "hdlr") {
                hdlr_ = new HdlrBox();
                p = hdlr_->Parse(p);
                media_type_ = hdlr_->media_type_;
            } else if (box_type == "minf") {
                minf_ = new MinfBox(media_type_);
                p = minf_->Parse(p);
            } else {
                std::cout << "unknown box type:" << box_type << "\r\n\r\n";
                p = start + box_size_;
                break;
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
        ss << "}";

        return ss.str();
    }
public:
    MdhdBox* mdhd_ = nullptr;
    HdlrBox* hdlr_ = nullptr;
    MinfBox* minf_ = nullptr;

    ENUM_MOV_MEDIA_TYPE media_type_ = UNKNOWN_BOX_TYPE;
};

//stts is in stbl
class SttsBox : public Mp4BoxBase
{
public:
    SttsBox() { type_ = "stts"; }
    ~SttsBox() {}
public:
    typedef struct SampleEntry_S
    {
        uint32_t first_chunk_;
        uint32_t samples_per_chunk_;
    } SampleEntry;

public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<SampleEntry> sample_entry;
};

//stss is in stbl
class StssBox : public Mp4BoxBase
{
public:
    StssBox() { type_ = "stss"; }
    ~StssBox() {}
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<uint32_t> samples_vec_;
};

//ctts is in stbl
class CttsBox : public Mp4BoxBase
{
public:
    CttsBox() { type_ = "ctts"; }
    ~CttsBox() {}

public:
    typedef struct SampleOffset_S {
        uint32_t sample_counts_;
        uint32_t sample_offsets_;
    } SampleOffset;
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
    typedef struct ChunkSample_S {
        uint32_t first_chunk_;
        uint32_t samples_per_chunk_;
        uint32_t sample_description_index_;
    } ChunkSample;
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<ChunkSample> chunk_sample_vec_;
};

//stsz is in stbl
class StszBox : public Mp4BoxBase
{
public:
    StszBox() { type_ = "stsz"; }
    ~StszBox() {}
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t constant_size_ = 0;
    uint32_t size_count_ = 0;
    std::vector<uint32_t> sample_sizes_vec_;
};

//stco is in stbl
class StcoBox : public Mp4BoxBase
{
public:
    StcoBox() { type_ = "stco"; }
    ~StcoBox() {}
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;
    std::vector<uint32_t> chunk_offsets_vec_;
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
        ss << "\"version\":" << (version_flag_ >> 24) << ",";
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
        }
        ss << "]";
        ss << "}";

        return ss.str();
    }
public:
    uint32_t version_flag_ = 0;//version: 8 bits, flag:24bits
    uint32_t entry_count_  = 0;

    std::vector<ElstInfo> elst_list_;
};

//edts is in trak
class EdtsBox : public Mp4BoxBase
{
public:
    EdtsBox() { type_ = "edts"; }
    ~EdtsBox() {}

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
private:
    ElstBox* elst_ = nullptr;
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
        ss << "\"data\":[";
        size_t i = 0;
        for (uint8_t byte_item : data_) {
            ss << (int)byte_item;
            if (i != data_.size() - 1) {
                ss << ",";
            }

            i++;
        }
        ss << "]}";

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
    ~UdtaBox() {}

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
    ~TrakBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        
        while(p < start + box_size_) {
            std::string box_type;
            int offset = 0;

            uint64_t box_size = GetBoxHeaderInfo(p, box_type, offset);
            if (box_type == "tkhd") {
                tkhd_ = new TkhdBox();
                p = tkhd_->Parse(p);
            } else if (box_type == "edts") {
                edts_ = new EdtsBox();
                p = edts_->Parse(p);
            } else if (box_type == "mdia") {
                mdia_ = new MdiaBox();
                p = mdia_->Parse(p);
            } else {
                std::cout << "unknown box type:" << box_type << "\r\n\r\n";
                p += box_size;
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
        ss << "}";

        return ss.str();
    }

public:
    TkhdBox* tkhd_ = nullptr;
    EdtsBox* edts_ = nullptr;
    MdiaBox* mdia_ = nullptr;
};

//moov is a root box
class MoovBox : public Mp4BoxBase
{
public:
    MoovBox() { type_ = "moov"; }
    ~MoovBox() {}

    uint8_t* Parse(uint8_t* start) {
        uint8_t* p = Mp4BoxBase::Parse(start);
        std::string box_type;
        int offset = 0;

        while (p < start + box_size_) {
            GetBoxHeaderInfo(p, box_type, offset);
            
            if (box_type == "mvhd") {
                mvhd_ = new MvhdBox();
                p = mvhd_->Parse(p);
            } else if (box_type == "trak") {
                TrakBox* trak = new TrakBox();
                p = trak->Parse(p);
                traks_.push_back(trak);
            } else if (box_type == "udta") {
                udta_ = new UdtaBox();
                p = udta_->Parse(p);
            } else {
                assert(0);
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
        ss << "}";
        
        return ss.str();
    }
public:
    MvhdBox* mvhd_ = nullptr;
    std::vector<TrakBox*> traks_;
    UdtaBox* udta_ = nullptr;
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

}

#endif//MP4_BOX_HPP