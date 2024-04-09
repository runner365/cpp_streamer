#include "flv_mux.hpp"
#include "flv_pub.hpp"
#include "logger.hpp"
#include "uuid.hpp"
#include "h264_h265_header.hpp"
#include <assert.h>

void* make_flvmux_streamer() {
    cpp_streamer::FlvMuxer* muxer = new cpp_streamer::FlvMuxer();

    return muxer;
}

void destroy_flvmux_streamer(void* streamer) {
    cpp_streamer::FlvMuxer* muxer = (cpp_streamer::FlvMuxer*)streamer;

    delete muxer;
}

namespace cpp_streamer
{
#define FLV_MUX_NAME "flvmux"

std::map<std::string, std::string> FlvMuxer::def_options_ = {
    {"onlyaudio", "false"},
    {"onlyvideo", "false"}
};

FlvMuxer::FlvMuxer()
{
    name_ = FLV_MUX_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
    options_ = def_options_;
}

FlvMuxer::~FlvMuxer()
{
}

std::string FlvMuxer::StreamerName() {
    return name_;
}

int FlvMuxer::AddSinker(CppStreamerInterface* sinker) {
    if (!sinker) {
        return sinkers_.size();
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int FlvMuxer::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

void FlvMuxer::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}

void FlvMuxer::Report(const std::string& type, const std::string& value) {
    if (report_) {
        report_->OnReport(name_, type, value);
    }
}
int FlvMuxer::SourceData(Media_Packet_Ptr pkt_ptr) {
    if (!pkt_ptr) {
        return 0;
    }

    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        uint8_t* p = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
        int len = pkt_ptr->buffer_ptr_->DataLen();

        if (len < 5) {
            LogErrorf(logger_, "flv mux input data len:%d error", len);
            Report("error", "video data length error");
            return -1;
        }
        int nalu_type_pos = GetNaluTypePos(p);
        if (nalu_type_pos < 3) {
            LogErrorf(logger_, "flv mux input data fail to find nalu start code");
            return -1;
        }

        if (len < (int)sizeof(sps_) && len < (int)sizeof(pps_)) {
            if (H264_IS_SPS(p[nalu_type_pos])) {
                memcpy(sps_, p + nalu_type_pos, len - nalu_type_pos);
                sps_len_ = len - nalu_type_pos;
                // LogInfoData(logger_, sps_, sps_len_, "sps data");
                return 0;
            }
            if (H264_IS_PPS(p[nalu_type_pos])) {
                memcpy(pps_, p + nalu_type_pos, len - nalu_type_pos);
                pps_len_ = len - nalu_type_pos;
                // LogInfoData(logger_, pps_, pps_len_, "pps data");
                return 0;
            }

        }
        if (pps_len_ <= 0 || sps_len_ <= 0) {
            return -1;
        }
        if (H264_IS_KEYFRAME(p[nalu_type_pos]) || !first_video_) {
            uint8_t extra_data[1024];
            int extra_len = 0;

            first_video_ = true;

            get_video_extradata(pps_, pps_len_, sps_, sps_len_,
                    extra_data, extra_len);

            Media_Packet_Ptr seq_ptr = std::make_shared<Media_Packet>();
            seq_ptr->copy_properties(*(pkt_ptr.get()));
            seq_ptr->is_seq_hdr_ = true;
            seq_ptr->is_key_frame_ = false;
            seq_ptr->buffer_ptr_->Reset();
            seq_ptr->buffer_ptr_->AppendData((char*)extra_data, extra_len);
            InputPacket(seq_ptr);
        }
        int nalu_len = len - nalu_type_pos;
        if (nalu_type_pos == 3) {
            pkt_ptr->buffer_ptr_->ConsumeData(-1);
            p--;
        }
        ByteStream::Write4Bytes(p, nalu_len);

        return InputPacket(pkt_ptr);
    }
    return InputPacket(pkt_ptr);
}

void FlvMuxer::StartNetwork(const std::string& url, void* loop_handle) {
}

void FlvMuxer::AddOption(const std::string& key, const std::string& value) {
    auto iter = options_.find(key);
    if (iter == options_.end()) {
        std::stringstream ss;
        ss << "the option key:" << key << " does not exist";
        throw CppStreamException(ss.str().c_str());
    }
    options_[key] = value;
    LogInfof(logger_, "set flvmuxer options key:%s, value:%s", key.c_str(), value.c_str());

    if (options_["onlyvideo"] == "true") {
        has_video_ = true;
        has_audio_ = false;
    }
    
    if (options_["onlyaudio"] == "true") {
        has_video_ = false;
        has_audio_ = true;
    }
}

int FlvMuxer::InputPacket(Media_Packet_Ptr pkt_ptr) {
    Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();

    if (!header_ready_) {
        header_ready_ = true;
        MuxFlvHeader(output_pkt_ptr);
    }

    size_t data_size = pkt_ptr->buffer_ptr_->DataLen();
    size_t media_size = 0;
    uint8_t header_data[16];

    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        //11 bytes header | 0x17 00 | 00 00 00 | data[...] | pre tag size
        media_size = 2 + 3 + data_size;
        header_data[0] = FLV_TAG_VIDEO;

    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        //11 bytes header | 0xaf 00 | data[...] | pre tag size
        media_size = 2 + data_size;

        header_data[0] = FLV_TAG_AUDIO;
    } else if (pkt_ptr->av_type_ == MEDIA_METADATA_TYPE) {
        header_data[0] = FLV_TAG_TYPE_META;
        media_size = data_size;
    } else {
        char error_sz[128];
        snprintf(error_sz, sizeof(error_sz), "flv mux does not suport media type:%d", (int)pkt_ptr->av_type_);
        LogWarnf(logger_, "flv mux does not suport media type:%d", (int)pkt_ptr->av_type_);
        Report("error", error_sz);
        return -1;
    }

    //Set DataSize(24)
    header_data[1] = (media_size >> 16) & 0xff;
    header_data[2] = (media_size >> 8) & 0xff;
    header_data[3] = media_size & 0xff;

    //Set Timestamp(24)|TimestampExtended(8)
    uint32_t timestamp_base = (uint32_t)(pkt_ptr->dts_ & 0xffffff);
    uint8_t timestamp_ext   = ((uint32_t)(pkt_ptr->dts_ & 0xffffff) >> 24) & 0xff;

    header_data[4] = (timestamp_base >> 16) & 0xff;
    header_data[5] = (timestamp_base >> 8) & 0xff;
    header_data[6] = timestamp_base & 0xff;
    header_data[7] = timestamp_ext & 0xff;

    //Set StreamID(24) as 1
    header_data[8]  = 0;
    header_data[9]  = 0;
    header_data[10] = 1;

    size_t header_size = 0;
    size_t pre_size = 0;

    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        //set media header
        header_size = 16;
        pre_size = 11 + 5 + pkt_ptr->buffer_ptr_->DataLen();
        if (pkt_ptr->is_seq_hdr_) {
            header_data[11] = FLV_VIDEO_KEY_FLAG;
            header_data[12] = FLV_VIDEO_AVC_SEQHDR;
        } else if (pkt_ptr->is_key_frame_) {
            header_data[11] = FLV_VIDEO_KEY_FLAG;
            header_data[12] = FLV_VIDEO_AVC_NALU;
        } else {
            header_data[11] = FLV_VIDEO_INTER_FLAG;
            header_data[12] = FLV_VIDEO_AVC_NALU;
        }
        if (pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
            header_data[11] |= FLV_VIDEO_H264_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_H265) {
            header_data[11] |= FLV_VIDEO_H265_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP8) {
            header_data[11] |= FLV_VIDEO_VP8_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP9) {
            header_data[11] |= FLV_VIDEO_VP9_CODEC;
        } else {
            char error_sz[128];
            snprintf(error_sz, sizeof(error_sz), "flv mux unsuport video codec type:%d", pkt_ptr->codec_type_);
            LogErrorf(logger_, "flv mux unsuport video codec type:%d", pkt_ptr->codec_type_);
            Report("error", error_sz);
            return -1;
        }
        uint32_t ts_delta = (pkt_ptr->pts_ > pkt_ptr->dts_) ? (pkt_ptr->pts_ - pkt_ptr->dts_) : 0;
        header_data[13] = (ts_delta >> 16) & 0xff;
        header_data[14] = (ts_delta >> 8) & 0xff;
        header_data[15] = ts_delta & 0xff;
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        header_size = 13;
        pre_size = 11 + 2 + pkt_ptr->buffer_ptr_->DataLen();

        header_data[11] = 0x0f;
        if (pkt_ptr->is_seq_hdr_) {
            header_data[12] = 0x00;
        } else {
            header_data[12] = 0x01;
        }
        if (pkt_ptr->codec_type_ == MEDIA_CODEC_AAC) {
            header_data[11] |= FLV_AUDIO_AAC_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_OPUS) {
            header_data[11] |= FLV_AUDIO_OPUS_CODEC;
        } else {
            char error_sz[128];
            snprintf(error_sz, sizeof(error_sz), "flv mux unsuport audio codec type:%d", pkt_ptr->codec_type_);
            LogErrorf(logger_, "flv mux unsuport audio codec type:%d", pkt_ptr->codec_type_);
            Report("error", error_sz);
            return -1;
        }
    } else if (pkt_ptr->av_type_ == MEDIA_METADATA_TYPE) {
        header_size = 11;
        pre_size = 11 + pkt_ptr->buffer_ptr_->DataLen();
    } else {
        char error_sz[128];
        snprintf(error_sz, sizeof(error_sz), "flv mux does not suport media type:%d", (int)pkt_ptr->av_type_);
        LogWarnf(logger_, "flv mux does not suport media type:%d", (int)pkt_ptr->av_type_);
        Report("error", error_sz);
        return -1;
    }

    output_pkt_ptr->buffer_ptr_->AppendData((char*)header_data, header_size);
    output_pkt_ptr->buffer_ptr_->AppendData(pkt_ptr->buffer_ptr_->Data(), pkt_ptr->buffer_ptr_->DataLen());

    uint8_t pre_tag_size_p[4];
    pre_tag_size_p[0] = (pre_size >> 24) & 0xff;
    pre_tag_size_p[1] = (pre_size >> 16) & 0xff;
    pre_tag_size_p[2] = (pre_size >> 8) & 0xff;
    pre_tag_size_p[3] = pre_size & 0xff;
    output_pkt_ptr->buffer_ptr_->AppendData((char*)pre_tag_size_p, sizeof(pre_tag_size_p));
    output_pkt_ptr->av_type_    = pkt_ptr->av_type_;
    output_pkt_ptr->codec_type_ = pkt_ptr->codec_type_;
    output_pkt_ptr->fmt_type_   = MEDIA_FORMAT_FLV;
    output_pkt_ptr->dts_  = pkt_ptr->dts_;
    output_pkt_ptr->pts_  = pkt_ptr->pts_;

    OutputPacket(output_pkt_ptr);
    return 0;
}

void FlvMuxer::OutputPacket(Media_Packet_Ptr pkt_ptr) {
    if (!pkt_ptr) {
        return;
    }

    for (auto& item : sinkers_) {
        item.second->SourceData(pkt_ptr);
    }
    return;
}

int FlvMuxer::MuxFlvHeader(Media_Packet_Ptr pkt_ptr) {
    uint8_t flag = 0;
    /*|'F'(8)|'L'(8)|'V'(8)|version(8)|TypeFlagsReserved(5)|TypeFlagsAudio(1)|TypeFlagsReserved(1)|TypeFlagsVideo(1)|DataOffset(32)|PreviousTagSize(32)|*/

    if (has_video_) {
        flag |= 0x01;
    }
    if (has_audio_) {
        flag |= 0x04;
    }
    uint8_t header_data[13] = {0x46, 0x4c, 0x56, 0x01, flag, 0x00, 0x00, 0x00, 0x09, 0, 0, 0, 0};

    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    pkt_ptr->buffer_ptr_->AppendData((char*)header_data, sizeof(header_data));

    return 0;
}

}
