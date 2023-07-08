#include "flv_demux.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "flv_pub.hpp"
#include "uuid.hpp"
#include "media_packet.hpp"
#include "amf0.hpp"
#include "audio_header.hpp"
#include "h264_h265_header.hpp"

#include <stdio.h>

void* make_flvdemux_streamer() {
    cpp_streamer::FlvDemuxer* demuxer = new cpp_streamer::FlvDemuxer();

    return demuxer;
}

void destroy_flvdemux_streamer(void* streamer) {
    cpp_streamer::FlvDemuxer* demuxer = (cpp_streamer::FlvDemuxer*)streamer;

    delete demuxer;
}

namespace cpp_streamer
{
#define FLV_DEMUX_NAME "flvdemux"

std::map<std::string, std::string> FlvDemuxer::def_options_ = {
    {"re", "false"}
};

FlvDemuxer::FlvDemuxer()
{
    name_ = FLV_DEMUX_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
    options_ = def_options_;
}

FlvDemuxer::~FlvDemuxer()
{
}

std::string FlvDemuxer::StreamerName() {
    return name_;
}

int FlvDemuxer::AddSinker(CppStreamerInterface* sinker) {
    if (!sinker) {
        return sinkers_.size();
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int FlvDemuxer::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

void FlvDemuxer::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}

int FlvDemuxer::SourceData(Media_Packet_Ptr pkt_ptr) {
    if (!pkt_ptr) {
        return 0;
    }

    return InputPacket(pkt_ptr);
}

void FlvDemuxer::AddOption(const std::string& key, const std::string& value) {
    auto iter = options_.find(key);
    if (iter == options_.end()) {
        std::stringstream ss;
        ss << "the option key:" << key << " does not exist";
        throw CppStreamException(ss.str().c_str());
    }
    options_[key] = value;
    LogInfof(logger_, "set options key:%s, value:%s", key.c_str(), value.c_str());
}

void FlvDemuxer::Report(const std::string& type, const std::string& value) {
    if (report_) {
        report_->OnReport(name_, type, value);
    }
}

int FlvDemuxer::HandlePacket() {
    uint8_t* p;
    uint32_t ts_delta = 0;
    bool skip_avcc_header = false;

    if (!flv_header_ready_) {
        if (!buffer_.Require(FLV_HEADER_LEN + FLV_TAG_PRE_SIZE)) {
            return FLV_RET_NEED_MORE;
        }

        p = (uint8_t*)buffer_.Data();

        if ((p[0] != 'F') || (p[1] != 'L') || (p[2] != 'V')) {
            Report("error", "flv header tag must be \"FLV\"");
            return -1;
        }
        if ((p[4] & 0x01) == 0x01) {
            has_video_ = true;
        }
        if ((p[4] & 0x04) == 0x04) {
            has_audio_ = true;
        }

        if ((p[5] != 0) || (p[6] != 0) || (p[7] != 0) || (p[8] != 9)) {
            Report("error", "flv pretag size error");
            return -1;
        }
        buffer_.ConsumeData(FLV_HEADER_LEN + FLV_TAG_PRE_SIZE);
        LogInfof(logger_, "flv has %s and %s", 
                has_video_ ? "video" : "no video",
                has_audio_ ? "audio" : "no audio");
        flv_header_ready_ = true;
    }

    if (!tag_header_ready_) {
        if (!buffer_.Require(FLV_TAG_HEADER_LEN)) {
            return FLV_RET_NEED_MORE;
        }
        p = (uint8_t*)buffer_.Data();
        tag_type_ = p[0];
        p++;
        tag_data_size_ = ByteStream::Read3Bytes(p);
        p += 3;
        tag_timestamp_ = ByteStream::Read3Bytes(p);
        p += 3;
        tag_timestamp_ |= ((uint32_t)p[0]) << 24;

        tag_header_ready_ = true;
        buffer_.ConsumeData(FLV_TAG_HEADER_LEN);
        //LogInfof(logger_, "p[0]:0x%02x.", *(uint8_t*)(buffer_.Data()));
    }

    if (!buffer_.Require(tag_data_size_ + FLV_TAG_PRE_SIZE)) {
        //LogInfof(logger_, "need more data");
        return FLV_RET_NEED_MORE;
    }
    p = (uint8_t*)buffer_.Data();

    Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
    bool is_ready = true;
    int header_len = 0;

    output_pkt_ptr->fmt_type_ = MEDIA_FORMAT_RAW;
    output_pkt_ptr->key_ = key_;
    if (tag_type_ == FLV_TAG_AUDIO) {
        header_len = 2;
        output_pkt_ptr->av_type_ = MEDIA_AUDIO_TYPE;

        if ((p[0] & 0xf0) == FLV_AUDIO_AAC_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_AAC;
        } else if ((p[0] & 0xf0) == FLV_AUDIO_OPUS_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_OPUS;
        } else if ((p[0] & 0xf0) == FLV_AUDIO_MP3_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_MP3;
        } else {
            is_ready = false;
            char error_sz[128];
            snprintf(error_sz, sizeof(error_sz), "does not suport audio codec type:0x%02x", p[0]);
            LogErrorf(logger_, error_sz);
            Report("error", error_sz);
            return -1;
        }
        if (p[1] == 0x00) {
            if ((p[0] & 0xf0) == FLV_AUDIO_AAC_CODEC) {
                LogInfoData(logger_, p, buffer_.DataLen(), "audio seq data");
                bool ret = GetAudioInfoByAsc(p + 2, output_pkt_ptr->buffer_ptr_->DataLen() - 2,
                                       output_pkt_ptr->aac_asc_type_, output_pkt_ptr->sample_rate_,
                                       output_pkt_ptr->channel_);
                if (ret) {
                    LogInfof(logger_, "decode asc to get codec type:%d, sample rate:%d, channel:%d",
                            output_pkt_ptr->codec_type_, output_pkt_ptr->sample_rate_,
                            output_pkt_ptr->channel_);
                }
                output_pkt_ptr->has_flv_audio_asc_ = true;
            }
            output_pkt_ptr->is_seq_hdr_ = true;
        } else {
            output_pkt_ptr->is_key_frame_ = true;
            output_pkt_ptr->is_seq_hdr_   = false;
            output_pkt_ptr->aac_asc_type_ = aac_asc_type_;
        }
    } else if (tag_type_ == FLV_TAG_VIDEO) {
        header_len = 2 + 3;
        output_pkt_ptr->av_type_ = MEDIA_VIDEO_TYPE;
        if ((p[0]&0x0f) == FLV_VIDEO_H264_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_H264;
        } else if ((p[0]&0x0f) == FLV_VIDEO_H265_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_H265;
        } else if ((p[0]&0x0f) == FLV_VIDEO_VP8_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_VP8;
        } else if ((p[0]&0x0f) == FLV_VIDEO_VP9_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_VP9;
        } else {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_UNKOWN;
            //is_ready = false;
            LogWarnf(logger_, "does not support codec type:0x%02x, tag type:%d, tag data size:%u, tag ts:%u",
                    p[0], tag_type_, tag_data_size_, tag_timestamp_);
            char error_sz[128];
            snprintf(error_sz, sizeof(error_sz), "does not suport video codec type:0x%02x", p[0]);
            Report("error", error_sz);
            LogErrorf(logger_, error_sz);
            //return -1;
        }

        if ((p[0] & 0xf0) == FLV_VIDEO_KEY_FLAG) {
            if (p[1] == 0x00) {
                output_pkt_ptr->is_seq_hdr_ = true;
            } else if (p[1] == 0x01) {
                output_pkt_ptr->is_seq_hdr_   = false;
                output_pkt_ptr->is_key_frame_ = true;
            } else {
                output_pkt_ptr->is_key_frame_ = false;
                output_pkt_ptr->is_seq_hdr_   = false;
                //is_ready = false;
                LogWarnf(logger_, "unkown data[1] byte:0x%02x, tag type:%d, tag data size:%u, tag ts:%u",
                        p[1], tag_type_, tag_data_size_, tag_timestamp_);
                //return -1;
            }
        }
        ts_delta = ByteStream::Read3Bytes(p + 2);

        if (output_pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
            uint8_t* nalu = p + header_len;

            if (output_pkt_ptr->is_seq_hdr_) {
                if (tag_data_size_ - header_len < 512) {
                    uint8_t sps[512];
                    uint8_t pps[512];
                    size_t sps_len = 0;
                    size_t pps_len = 0;
                    uint8_t start_code[4] = {0, 0, 0, 1};

                    skip_avcc_header = true;
                    output_pkt_ptr->dts_ = tag_timestamp_;
                    output_pkt_ptr->pts_ = tag_timestamp_ + ts_delta;

                    GetSpsPpsFromExtraData(pps, pps_len, 
                            sps, sps_len, 
                            nalu, tag_data_size_ - header_len);

                    Media_Packet_Ptr sps_ptr = std::make_shared<Media_Packet>();
                    Media_Packet_Ptr pps_ptr = std::make_shared<Media_Packet>();

                    sps_ptr->copy_properties(output_pkt_ptr);
                    pps_ptr->copy_properties(output_pkt_ptr);

                    sps_ptr->is_seq_hdr_ = true;
                    pps_ptr->is_seq_hdr_ = true;

                    sps_ptr->is_key_frame_ = false;
                    pps_ptr->is_key_frame_ = false;

                    sps_ptr->buffer_ptr_->AppendData((char*)start_code, sizeof(start_code));
                    pps_ptr->buffer_ptr_->AppendData((char*)start_code, sizeof(start_code));

                    sps_ptr->buffer_ptr_->AppendData((char*)sps, sps_len);
                    pps_ptr->buffer_ptr_->AppendData((char*)pps, pps_len);

                    output_pkt_ptr->buffer_ptr_->AppendData((char*)p + header_len, tag_data_size_ - header_len);
                    SinkData(sps_ptr);
                    SinkData(pps_ptr);
                }
           }
        }
    } else if (tag_type_ == FLV_TAG_TYPE_META) {
        output_pkt_ptr->av_type_ = MEDIA_METADATA_TYPE;
        if (DecodeMetaData(p, tag_data_size_, output_pkt_ptr) < 0) {
            is_ready = false;
            buffer_.ConsumeData(tag_data_size_ + FLV_TAG_PRE_SIZE);
            tag_header_ready_ = false;
            Report("error", "decode metadata error");
            return 0;
        }
    } else {
        is_ready = false;
        buffer_.ConsumeData(tag_data_size_ + FLV_TAG_PRE_SIZE);
        tag_header_ready_ = false;
        LogErrorf(logger_, "does not suport tag type:0x%02x", tag_type_);
        return 0;
    }

    if (is_ready && ((int)tag_data_size_ > header_len)) {
        if (!skip_avcc_header) {
            output_pkt_ptr->dts_ = tag_timestamp_;
            output_pkt_ptr->pts_ = tag_timestamp_ + ts_delta;

            if (output_pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
                uint8_t* nalu_data = p + header_len;
                size_t nalus_len = tag_data_size_ - header_len;
                std::vector<std::shared_ptr<DataBuffer>> nalus;

                Avcc2Nalus(nalu_data, nalus_len, nalus);
                for(auto data_ptr : nalus) {
                    nalu_data = (uint8_t*)data_ptr->Data();
                    size_t nalu_len = data_ptr->DataLen();

                    int pos = GetNaluTypePos(nalu_data);
                    if (pos < 3) {
                        LogErrorf(logger_, "nalu type pos error:%d", pos);
                        continue;
                    }
                    Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>();
                    pkt_ptr->copy_properties(output_pkt_ptr);

                    if (H264_IS_PPS(nalu_data[pos]) || H264_IS_SPS(nalu_data[pos])) {
                        pkt_ptr->is_seq_hdr_ = true;
                        pkt_ptr->is_key_frame_ = false;
                    }
                    if (H264_IS_KEYFRAME(nalu_data[pos])) {
                        pkt_ptr->is_seq_hdr_ = false;
                        pkt_ptr->is_key_frame_ = true;
                    }
                    pkt_ptr->buffer_ptr_->AppendData((char*)nalu_data, nalu_len);
                    SinkData(pkt_ptr);
                }
            } else {
                output_pkt_ptr->buffer_ptr_->AppendData((char*)p + header_len, tag_data_size_ - header_len);
                SinkData(output_pkt_ptr);
            }
        }
    }

    buffer_.ConsumeData(tag_data_size_ + FLV_TAG_PRE_SIZE);
    tag_header_ready_ = false;
    return 0;
}

int FlvDemuxer::DecodeMetaData(uint8_t* data, int data_len, Media_Packet_Ptr pkt_ptr) {
    AMF_ITERM item;
    int index = 0;

    do {
        if (AMF_Decoder::Decode(data, data_len, item) < 0) {
            LogWarnf(logger_,"metadata decode return data len:%d", data_len);
            return 0;
        }
        if (index == 0) {
            if (item.GetAmfType() != AMF_DATA_TYPE_STRING) {
                LogErrorf(logger_,"metadata must be string type, the amf type:%d, number:%f", item.GetAmfType(), item.number_);
                return -1;
            }

            if (item.desc_str_ == "onTextData") {
                pkt_ptr->metadata_type_ = METADATA_TYPE_ONTEXTDATA;
            } else if (item.desc_str_ == "onCaption") {
                pkt_ptr->metadata_type_ = METADATA_TYPE_ONCAPTION;
            } else if (item.desc_str_ == "onCaptionInfo") {
                pkt_ptr->metadata_type_ = METADATA_TYPE_ONCAPTIONINFO;
            } else if (item.desc_str_ == "onMetaData") {
                pkt_ptr->metadata_type_ = METADATA_TYPE_ONTMETADATA;
            } else {
                pkt_ptr->metadata_type_ = METADATA_TYPE_UNKNOWN;
                LogErrorf(logger_, "unknown metadata type:%s", item.desc_str_.c_str());
            }
        } else {
            if (item.GetAmfType() == AMF_DATA_TYPE_OBJECT) {
                for (auto& amf_obj : item.amf_obj_) {
                    std::string key = amf_obj.first;
                    if (amf_obj.second->GetAmfType() == AMF_DATA_TYPE_STRING) {
                        pkt_ptr->metadata_[key] = amf_obj.second->desc_str_;
                    } else if (amf_obj.second->GetAmfType() == AMF_DATA_TYPE_NUMBER) {
                        char desc[80];
                        snprintf(desc, sizeof(desc), "%.02f", amf_obj.second->number_);
                        pkt_ptr->metadata_[key] = std::string(desc);
                    } else if (amf_obj.second->GetAmfType() == AMF_DATA_TYPE_BOOL) {
                        pkt_ptr->metadata_[key] = amf_obj.second->enable_ ? "true" : "false";
                    }
                }
            }
        }
        index++;
    } while (data_len > 1);

    return 0;
}

int FlvDemuxer::SinkData(Media_Packet_Ptr pkt_ptr) {
    if (options_["re"] == "true") {
        waiter_.Wait(pkt_ptr);
    }
    int ret = 0;
    for (auto& item : sinkers_) {
        ret += item.second->SourceData(pkt_ptr);
    }
    return ret;
}

int FlvDemuxer::InputPacket(Media_Packet_Ptr pkt_ptr) {
    buffer_.AppendData(pkt_ptr->buffer_ptr_->Data(), pkt_ptr->buffer_ptr_->DataLen());
    if (key_.empty() && !pkt_ptr->key_.empty()) {
        key_ = pkt_ptr->key_;
    }
    int ret = 0;
    do {
        ret = HandlePacket();
    } while (ret == 0);
    
    return ret;
}

int FlvDemuxer::InputPacket(const uint8_t* data, size_t data_len, const std::string& key) {
    buffer_.AppendData((char*)data, data_len);
    key_ = key;

    int ret = 0;
    do {
        ret = HandlePacket();
    } while (ret == 0);
    
    return ret;
}

}
