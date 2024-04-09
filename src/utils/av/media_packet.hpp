#ifndef MEDIA_PACKET_HPP
#define MEDIA_PACKET_HPP
#include "av.hpp"
#include "data_buffer.hpp"

#include <stdint.h>
#include <string>
#include <memory>
#include <sstream>
#include <map>

namespace cpp_streamer
{

class Media_Packet
{
public:
    Media_Packet()
    {
        buffer_ptr_ = std::make_shared<DataBuffer>();
    }
    Media_Packet(size_t len)
    {
        buffer_ptr_ = std::make_shared<DataBuffer>(len);
    }
    Media_Packet(const Media_Packet& input_packet)
    {
        copy_properties(input_packet);
        buffer_ptr_ = std::make_shared<DataBuffer>(input_packet.buffer_ptr_->DataLen() + 1024);
        buffer_ptr_->AppendData(input_packet.buffer_ptr_->Data(),
                input_packet.buffer_ptr_->DataLen());
    }
    Media_Packet& operator=(const Media_Packet& input_packet)
    {
        copy_properties(input_packet);
        buffer_ptr_ = std::make_shared<DataBuffer>(input_packet.buffer_ptr_->DataLen() + 1024);
        buffer_ptr_->AppendData(input_packet.buffer_ptr_->Data(),
                input_packet.buffer_ptr_->DataLen());
        return *this;
    }
    ~Media_Packet()
    {
    }

    std::shared_ptr<Media_Packet> copy() {
        std::shared_ptr<Media_Packet> pkt_ptr = std::make_shared<Media_Packet>(this->buffer_ptr_->DataLen() + 1024);

        pkt_ptr->copy_properties(*this);
        pkt_ptr->buffer_ptr_->AppendData(this->buffer_ptr_->Data(), this->buffer_ptr_->DataLen());
        return pkt_ptr;
    }

    void copy_properties(const Media_Packet& pkt) {
        this->av_type_      = pkt.av_type_;
        this->codec_type_   = pkt.codec_type_;
        this->fmt_type_     = pkt.fmt_type_;
        this->dts_          = pkt.dts_;
        this->pts_          = pkt.pts_;
        this->is_key_frame_ = pkt.is_key_frame_;
        this->is_seq_hdr_   = pkt.is_seq_hdr_;

        this->key_        = pkt.key_;
        this->vhost_      = pkt.vhost_;
        this->app_        = pkt.app_;
        this->streamname_ = pkt.streamname_;
        this->streamid_   = pkt.streamid_;
        this->typeid_     = pkt.typeid_;
    }

    void copy_properties(const std::shared_ptr<Media_Packet> pkt_ptr) {
        this->av_type_      = pkt_ptr->av_type_;
        this->codec_type_   = pkt_ptr->codec_type_;
        this->fmt_type_     = pkt_ptr->fmt_type_;
        this->dts_          = pkt_ptr->dts_;
        this->pts_          = pkt_ptr->pts_;
        this->is_key_frame_ = pkt_ptr->is_key_frame_;
        this->is_seq_hdr_   = pkt_ptr->is_seq_hdr_;

        this->key_        = pkt_ptr->key_;
        this->vhost_      = pkt_ptr->vhost_;
        this->app_        = pkt_ptr->app_;
        this->streamname_ = pkt_ptr->streamname_;
        this->streamid_   = pkt_ptr->streamid_;
        this->typeid_     = pkt_ptr->typeid_;
    }

    std::string Dump(bool data_dump = false) {
        std::stringstream ss;
        
        ss << "av type:" << avtype_tostring(av_type_);

        if (av_type_ != MEDIA_METADATA_TYPE) {
            ss << ", codec type:" << codectype_tostring(codec_type_);
        }
        ss << ", format type:" << formattype_tostring(fmt_type_) << ", dts:" << dts_ << ", pts:" << pts_
           << ", is key frame:" << is_key_frame_ << ", is seq frame:" << is_seq_hdr_
           << ", data length:" << buffer_ptr_->DataLen();
        if (!key_.empty()) {
            ss << ", key:" << key_;
        }
        if (!app_.empty()) {
            ss << ", app:" << app_;
        }
        if (!streamname_.empty()) {
            ss << ", stream name:" << streamname_;
        }
        if (!metadata_.empty()) {
            ss << "\r\n";
            ss << "metadata type:" << metadata_type_ << "\r\n";
            for (auto& item : metadata_) {
                ss << "key:" << item.first << ", value:" << item.second << "\r\n";
            }
        }
        if (data_dump) {
            uint8_t* data = (uint8_t*)buffer_ptr_->Data();
            char print_buffer[2048];
            size_t print_len = 0;

            ss << "\r\ndata:";
            for (size_t i = 0; i < buffer_ptr_->DataLen(); i++) {
                if (i % 16 == 0) {
                    print_len += snprintf(print_buffer + print_len, sizeof(print_buffer) - print_len, "\r\n");
                    ss << print_buffer;
                    print_len = 0;
                }
                print_len += snprintf(print_buffer + print_len, sizeof(print_buffer) - print_len, "%02x ", data[i]);
            }
            if (print_len > 0) {
                ss << print_buffer;
            }
            ss << "\r\n";
        }
        return ss.str();
    }

public:
    MEDIA_PKT_TYPE av_type_      = MEDIA_UNKOWN_TYPE;
    MEDIA_CODEC_TYPE codec_type_ = MEDIA_CODEC_UNKOWN;
    MEDIA_FORMAT_TYPE fmt_type_  = MEDIA_FORMAT_UNKOWN;
    int64_t dts_ = -1;
    int64_t pts_ = -1;
    bool is_key_frame_ = false;
    bool is_seq_hdr_   = false;
    bool has_flv_audio_asc_ = false;
    std::shared_ptr<DataBuffer> buffer_ptr_;
    int metadata_type_;
    std::map<std::string, std::string> metadata_;

public:
    int sample_rate_ = 44100;
    int sample_size_ = 1;
    uint8_t channel_ = 2;
    uint8_t aac_asc_type_ = ASC_TYPE_AAC_LC;
//rtmp info:
public:
    std::string key_;//vhost(option)_appname_streamname
    std::string vhost_;
    std::string app_;
    std::string streamname_;
    uint32_t streamid_ = 0;
    uint8_t typeid_ = 0;
//mp4 info
public:
    std::string box_type_;
    void* box_ = nullptr;
    IoReadInterface* io_reader_ = nullptr;
};

typedef std::shared_ptr<Media_Packet> Media_Packet_Ptr;

class av_writer_base
{
public:
    virtual int write_packet(Media_Packet_Ptr) = 0;
    virtual std::string get_key() = 0;
    virtual std::string get_writerid() = 0;
    virtual void close_writer() = 0;
    virtual bool is_inited() = 0;
    virtual void set_init_flag(bool flag) = 0;
};

}
#endif//MEDIA_PACKET_HPP
