#include "mp4_demux.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "uuid.hpp"
#include "media_packet.hpp"
#include "audio_header.hpp"
#include "h264_h265_header.hpp"
#include "stringex.hpp"

#include <stdio.h>

#define MP4_DEMUX_NAME "mp4demux"

void* make_mp4demux_streamer() {
    cpp_streamer::Mp4Demuxer* demuxer = new cpp_streamer::Mp4Demuxer();

    return demuxer;
}

void destroy_mp4demux_streamer(void* streamer) {
    cpp_streamer::Mp4Demuxer* demuxer = (cpp_streamer::Mp4Demuxer*)streamer;

    delete demuxer;
}

namespace cpp_streamer
{
std::map<std::string, std::string> Mp4Demuxer::def_options_ = {
    {"re", "false"},
    {"box_detail", "false"}
};

static std::string MovItemDump(const MovItem* item) {
    std::stringstream ss;
    std::string av_type;

    if (item->av_type_ == MEDIA_AUDIO_TYPE) {
        av_type = "audio";
    } else if (item->av_type_ == MEDIA_VIDEO_TYPE) {
        av_type = "video";
    } else {
        av_type = "unknown";
    }
    int64_t dts = (item->timescale_ > 0) ? item->dts * 1000 / item->timescale_ : item->dts;
    int64_t pts = (item->timescale_ > 0) ? item->pts * 1000 / item->timescale_ : item->pts;

    ss << "{";
    ss << "\"av_type\":\"" << av_type << "\",";
    ss << "\"offset\":" << item->offset << ",";
    ss << "\"len\":" << item->len << ",";
    ss << "\"dts\":" << dts << ",";
    ss << "\"pts\":" << pts << ",";
    ss << "\"keyframe\":" << item->is_keyframe_;
    ss << "}";
    return ss.str();
}

Mp4Demuxer::Mp4Demuxer()
{
    name_ = MP4_DEMUX_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
    options_ = def_options_;
}

Mp4Demuxer::~Mp4Demuxer()
{
    if (ftyp_box_) {
        delete ftyp_box_;
        ftyp_box_ = nullptr;
    }
    if (moov_box_) {
        delete moov_box_;
        moov_box_ = nullptr;
    }
    if (free_box_) {
        delete free_box_;
        free_box_ = nullptr;
    }
    if (mdat_box_) {
        delete mdat_box_;
        mdat_box_ = nullptr;
    }
    for (Mp4BoxBase* box : unknown_boxes_) {
        delete box;
    }
    unknown_boxes_.clear();
}

std::string Mp4Demuxer::StreamerName() {
    return name_;
}

int Mp4Demuxer::AddSinker(CppStreamerInterface* sinker) {
    if (sinker == nullptr) {
        return 0;
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int Mp4Demuxer::RemoveSinker(const std::string& name) {
    return 0;
}

void Mp4Demuxer::OnRead() {
    const int min_size = 16;
    int64_t offset = 0;
    std::vector<uint8_t> buffer;
    
    buffer.resize(min_size);

    while (true) {
        uint8_t* p = (uint8_t*)&buffer[0];
        int ret = io_reader_->Read(offset, p, min_size);
        if (ret < min_size) {
            return;
        }
        std::string box_type;
        int mov_offset = 0;
        uint64_t box_size = GetBoxHeaderInfo(p, box_type, mov_offset);
        if (box_size > buffer.size()) {
            buffer.resize(box_size);
        }
        p = (uint8_t*)&buffer[0];
        ret = io_reader_->Read(offset, p, box_size);
        if (ret < box_size) {
            return;
        }
        offset += box_size;

        if (box_type == "ftyp") {
            ftyp_box_ = new FtypBox();
            p = ftyp_box_->Parse(p, mov_);

            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box_type;
                output_pkt_ptr->box_ = (void*)ftyp_box_;
                Output(output_pkt_ptr);
            }
        } else if (box_type == "moov") {
            moov_box_ = new MoovBox();
            p = moov_box_->Parse(p, mov_);

            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box_type;
                output_pkt_ptr->box_ = (void*)moov_box_;
                Output(output_pkt_ptr);
            }

            makeMovItems();

            adjustAllDts();

            handleMovItems();
        } else if (box_type == "free") {
            free_box_ = new FreeBox();
            p = free_box_->Parse(p);

            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box_type;
                output_pkt_ptr->box_ = (void*)free_box_;
                Output(output_pkt_ptr);
            }
        } else if (box_type == "mdat") {
            mdat_box_ = new MdatBox();
            p = mdat_box_->Parse(p, mov_);

            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box_type;
                output_pkt_ptr->box_ = (void*)mdat_box_;
                Output(output_pkt_ptr);
            }
        } else {
            Mp4BoxBase* box = new Mp4BoxBase();
            box->Parse(p);
            p += box->box_size_;
            unknown_boxes_.push_back(box);

            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box->type_;
                output_pkt_ptr->box_ = (void*)box;
                Output(output_pkt_ptr);
            }
        }
    }
}

int Mp4Demuxer::SourceData(Media_Packet_Ptr pkt_ptr) {
    if (pkt_ptr->io_reader_) {
        io_reader_ = pkt_ptr->io_reader_;
        OnRead();
        return 0;
    }
    buffer_.AppendData(pkt_ptr->buffer_ptr_->Data(), pkt_ptr->buffer_ptr_->DataLen());

    uint8_t* p = (uint8_t*)buffer_.Data();
    while (p < (uint8_t*)(buffer_.Data() + buffer_.DataLen())) {
        std::string box_type;
        int offset = 0;
        uint64_t box_size = GetBoxHeaderInfo(p, box_type, offset);

        if ((p + box_size) > (uint8_t*)(buffer_.Data() + buffer_.DataLen())) {
            break;
        }
        (void)offset;

        if (box_type == "ftyp") {
            ftyp_box_ = new FtypBox();
            p = ftyp_box_->Parse(p, mov_);

            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box_type;
                output_pkt_ptr->box_ = (void*)ftyp_box_;
                Output(output_pkt_ptr);
            }
        } else if (box_type == "moov") {
            moov_box_ = new MoovBox();
            p = moov_box_->Parse(p, mov_);
            
            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box_type;
                output_pkt_ptr->box_ = (void*)moov_box_;
                Output(output_pkt_ptr);
            }

            makeMovItems();

            adjustAllDts();

            handleMovItems();
        } else if (box_type == "free") {
            free_box_ = new FreeBox();
            p = free_box_->Parse(p);

            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box_type;
                output_pkt_ptr->box_ = (void*)free_box_;
                Output(output_pkt_ptr);
            }
        } else if (box_type == "mdat") {
            mdat_box_ = new MdatBox();
            p = mdat_box_->Parse(p, mov_);

            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box_type;
                output_pkt_ptr->box_ = (void*)mdat_box_;
                Output(output_pkt_ptr);
            }
        } else {
            Mp4BoxBase* box = new Mp4BoxBase();
            box->Parse(p);
            p += box->box_size_;
            unknown_boxes_.push_back(box);

            if (options_["box_detail"] == "true") {
                Media_Packet_Ptr output_pkt_ptr = std::make_shared<Media_Packet>();
                output_pkt_ptr->av_type_ = MEDIA_MOVBOX_TYPE;
                output_pkt_ptr->box_type_ = box->type_;
                output_pkt_ptr->box_ = (void*)box;
                Output(output_pkt_ptr);
            }
        }
    }
    buffer_.ConsumeData(p - (uint8_t*)buffer_.Data());
    return 0;
}

void Mp4Demuxer::handleMovItems() {
    std::vector<uint8_t> data_buffer;
    data_buffer.resize(10 * 1024);

    for (auto item : sample_items_) {
        if (item.second.len > data_buffer.size()) {
            data_buffer.resize(item.second.len);
        }
        uint8_t* data = (uint8_t*)(&data_buffer[0]);

        int ret = io_reader_->Read(item.second.offset, data, item.second.len);
        if (ret < item.second.len) {
            LogWarnf(logger_, "io read return:%d, read len:%d", ret, item.second.len);
            break;
        }
        int64_t dts = item.second.timescale_ > 0 ? item.second.dts * 1000 / item.second.timescale_ : item.second.dts;
        int64_t pts = item.second.timescale_ > 0 ? item.second.pts * 1000 / item.second.timescale_ : item.second.pts;

        // Avcc to AnnexB
        if (item.second.av_type_ == MEDIA_VIDEO_TYPE) {
            if (item.second.codec_type_ == MEDIA_CODEC_H264
                || item.second.codec_type_ == MEDIA_CODEC_H265) {
                if (item.second.len <= 5) {
                    continue;
                }
                std::vector<std::shared_ptr<DataBuffer>> nalus;
                bool ret = Avcc2Nalus(data, item.second.len, nalus);
                if (!ret) {
                    LogErrorf(logger_, "avcc to nalus error");
                    continue;
                }
                if (nalus.empty()) {
                    LogErrorf(logger_, "avcc to nalus error: nalus is empty");
                    continue;
                }

                for (std::shared_ptr<DataBuffer> db_ptr : nalus) {
                    
                    uint8_t* nalu_data = (uint8_t*)db_ptr->Data();

                    nalu_data[0] = 0;
                    nalu_data[1] = 0;
                    nalu_data[2] = 0;
                    nalu_data[3] = 1;

                    if (H264_IS_SPS(nalu_data[4]) || H264_IS_PPS(nalu_data[4])) {
                        sendMediaPacket(item.second.av_type_,
                            item.second.codec_type_,
                            dts, pts,
                            false, true,
                            nalu_data, db_ptr->DataLen());
                        continue;
                    }

                    if (H264_IS_KEYFRAME(nalu_data[4])) {
                        sendMediaPacket(item.second.av_type_,
                            item.second.codec_type_,
                            dts, pts,
                            true, false,
                            nalu_data, db_ptr->DataLen());
                        continue;
                    }

                    sendMediaPacket(item.second.av_type_,
                        item.second.codec_type_,
                        dts, pts,
                        false, false,
                        nalu_data, db_ptr->DataLen());
                }
                continue;
            }
        }

        sendMediaPacket(item.second.av_type_,
            item.second.codec_type_,
            dts, pts,
            false, false,
            data, item.second.len);
    }
}

void Mp4Demuxer::sendMediaPacket(MEDIA_PKT_TYPE av_type, MEDIA_CODEC_TYPE codec_type, int64_t dts, int64_t pts,
    bool is_keyframe, bool is_seqhdr, uint8_t* data, size_t len) {
    Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>(len);
    pkt_ptr->av_type_ = av_type,
    pkt_ptr->codec_type_ = codec_type;
    pkt_ptr->fmt_type_ = MEDIA_FORMAT_RAW;
    pkt_ptr->dts_ = dts;
    pkt_ptr->pts_ = pts;
    pkt_ptr->is_key_frame_ = is_keyframe;
    pkt_ptr->is_seq_hdr_ = is_seqhdr;
    pkt_ptr->buffer_ptr_->AppendData((char*)data, len);
    Output(pkt_ptr);
}

void Mp4Demuxer::adjustAllDts() {
    int64_t v_dts = -1;
    int64_t a_dts = -1;
    bool update_video = false;
    double v_duration_ = 0.0;
    double a_duration_ = 0.0;

    for (const TrakInfo& trak : mov_.traks_info_) {
        if (trak.handler_type_ == "soun") {
            if (trak.sample_entries_.size() > 0 && trak.timescale_ > 0) {
                a_duration_ = trak.sample_entries_[0].samples_delta_ * 1000000 / trak.timescale_; 
            }
            continue;
        }
        if (trak.handler_type_ == "vide") {
            if (trak.sample_entries_.size() > 0 && trak.timescale_ > 0) {
                v_duration_ = trak.sample_entries_[0].samples_delta_ * 1000000 / trak.timescale_; 
            }
            continue;
        }
    }

    for (const auto& item : sample_items_) {
        if (item.second.av_type_ == MEDIA_AUDIO_TYPE) {
            if (v_dts > 0 && a_dts <= 0) {
                a_dts = v_dts;
                a_dts += a_duration_;
                update_video = false;
                break;
            }
            a_dts = item.second.dts;
        }
        if (item.second.av_type_ == MEDIA_VIDEO_TYPE) {
            if (a_dts > 0 && v_dts <= 0) {
                v_dts = a_dts;
                v_dts += v_duration_;
                update_video = true;
                break;
            }
            v_dts = item.second.dts;
        }
    }
    for (auto& item : sample_items_) {
        if (item.second.av_type_ == MEDIA_AUDIO_TYPE) {
            if (!update_video) {
                item.second.dts += a_dts;
                item.second.pts += a_dts;
            }
        }
        if (item.second.av_type_ == MEDIA_VIDEO_TYPE) {
            if (update_video) {
                item.second.dts += v_dts;
                item.second.pts += v_dts;
            }
        }
    }
}

int64_t Mp4Demuxer::getDurationBySampleIndex(uint32_t sample_index,
        const TrakInfo& trakinfo) {
    uint32_t sample_count = 0;
    int64_t duration = 0;

    for (const SampleEntry& entry : trakinfo.sample_entries_) {
        uint32_t next = sample_count + entry.sample_count_;

        if (sample_index >= sample_count && sample_index < next) {
            duration = entry.samples_delta_;
            break;
        }

        sample_count += entry.sample_count_;
    }
    return duration;
}

int64_t Mp4Demuxer::getPtsBySampleIndex(uint32_t sample_index, int64_t dts,
        const TrakInfo& trakinfo) {
    uint32_t sample_count = 0;
    uint32_t cts = 0;

    for (const SampleOffset item : trakinfo.sample_offset_vec_) {
        sample_count += item.sample_counts_;

        if (sample_count == sample_index) {
            cts = item.sample_offsets_;
            break;
        }
    }
    return dts + cts;
}

bool Mp4Demuxer::isKeyframe(uint32_t sample_index, const TrakInfo& trakinfo) {
    for (uint32_t keyframe_pos : trakinfo.iframe_sample_vec_) {
        if (sample_index == keyframe_pos) {
            return true;
        }
    }
    return false;
}

void Mp4Demuxer::getSampleInfoInChunk(uint32_t chunk_offset, size_t chunk_index,
    uint32_t sample_per_chunk, uint32_t start_sample_index, int64_t& dts, const TrakInfo& trakinfo) {
    size_t sample_offset = chunk_offset;
    MEDIA_PKT_TYPE av_type;

    if (trakinfo.handler_type_ == "soun") {
        av_type = MEDIA_AUDIO_TYPE;
    } else if (trakinfo.handler_type_ == "vide") {
        av_type = MEDIA_VIDEO_TYPE;
    } else {
        return;
    }

    for (uint32_t i = 0; i < sample_per_chunk; i++) {
        uint32_t sample_index = start_sample_index + i;
        uint32_t sample_size = trakinfo.sample_sizes_vec_[sample_index - 1];
        int64_t duration = getDurationBySampleIndex(sample_index, trakinfo);
        int64_t pts = getPtsBySampleIndex(sample_index, dts, trakinfo);
        bool keyFrame = isKeyframe(sample_index, trakinfo);

        MovItem item = {
            .av_type_ = av_type,
            .codec_type_ = trakinfo.codec_type_,
            .offset = sample_offset,
            .len = sample_size,
            .dts = dts,
            .pts = pts,
            .timescale_ = 1000000,
            .is_keyframe_ = keyFrame
        };
        if (trakinfo.timescale_ > 0) {
            item.dts = item.dts * 1000000 / trakinfo.timescale_;
            item.pts = item.pts * 1000000 / trakinfo.timescale_;
        }

        std::string dump = MovItemDump(&item);

        sample_items_.insert(std::make_pair(item.dts, item));

        /****** end doing *****/
        sample_offset += sample_size;

        dts += duration;
    }
}

void Mp4Demuxer::handleAACExtraData(const TrakInfo& trakinfo) {
    if (trakinfo.handler_type_ != "soun") {
        return;
    }
    if (trakinfo.codec_type_ != MEDIA_CODEC_AAC) {
        return;
    }
    uint8_t* data = (uint8_t*)(&trakinfo.sequence_data_[0]);
    size_t data_len = trakinfo.sequence_data_.size();

    sendMediaPacket(MEDIA_AUDIO_TYPE,
            trakinfo.codec_type_,
            0, 0,
            false, true,
            data, data_len);
}

void Mp4Demuxer::handleH265VpsSpsPps(const TrakInfo& trakinfo) {
    if (trakinfo.handler_type_ != "vide") {
        CSM_THROW_ERROR("wrong media type(%s) exception", trakinfo.handler_type_.c_str());
    }
    if (trakinfo.codec_type_ != MEDIA_CODEC_H265) {
        CSM_THROW_ERROR("wrong codec type(%s) exception", codectype_tostring(trakinfo.codec_type_).c_str());
        return;
    }
    uint8_t* extra_data = (uint8_t*)(&trakinfo.sequence_data_[0]);
    int extra_len = (int)trakinfo.sequence_data_.size();
    HEVC_DEC_CONF_RECORD hevc_dec_info;
    uint8_t vps[2048];
    uint8_t sps[2048];
    uint8_t pps[2048];
    size_t vps_len = 0;
    size_t sps_len = 0;
    size_t pps_len = 0;

    GetHevcDecInfoFromExtradata(&hevc_dec_info, extra_data, extra_len);

    GetVpsSpsPpsFromHevcDecInfo(&hevc_dec_info, vps + 5, vps_len, sps + 5, sps_len, pps + 5, pps_len);

    vps[0] = 0;
    vps[1] = 0;
    vps[2] = 0;
    vps[3] = 1;

    sps[0] = 0;
    sps[1] = 0;
    sps[2] = 0;
    sps[3] = 1;

    pps[0] = 0;
    pps[1] = 0;
    pps[2] = 0;
    pps[3] = 1;

    for (const HEVC_NALUnit& item : hevc_dec_info.nalu_vec) {
        if (item.nal_unit_type == NAL_UNIT_VPS) {
            vps[4] = item.nal_unit_type;
        } else if (item.nal_unit_type == NAL_UNIT_SPS) {
            sps[4] = item.nal_unit_type;
        } else if (item.nal_unit_type == NAL_UNIT_PPS) {
            pps[4] = item.nal_unit_type;
        } else {
            continue;
        }
    }

    sendMediaPacket(MEDIA_VIDEO_TYPE, trakinfo.codec_type_,
        0, 0,
        false, true,
        vps, vps_len);
    sendMediaPacket(MEDIA_VIDEO_TYPE, trakinfo.codec_type_,
        0, 0,
        false, true,
        sps, sps_len);
    sendMediaPacket(MEDIA_VIDEO_TYPE, trakinfo.codec_type_,
        0, 0,
        false, true,
        pps, pps_len);
}

void Mp4Demuxer::handleH264SpsPps(const TrakInfo& trakinfo) {
    if (trakinfo.handler_type_ != "vide") {
        CSM_THROW_ERROR("wrong media type(%s) exception", trakinfo.handler_type_.c_str());
    }
    if (trakinfo.codec_type_ != MEDIA_CODEC_H264) {
        CSM_THROW_ERROR("wrong codec type(%s) exception", codectype_tostring(trakinfo.codec_type_).c_str());
        return;
    }

    char* data = (char*)(&trakinfo.sequence_data_[0]);
    uint8_t pps[2048];
    uint8_t sps[2048];
    size_t pps_len = 0;
    size_t sps_len = 0;

    int ret = GetSpsPpsFromExtraData(pps + 4, pps_len, sps + 4, sps_len,
        (uint8_t*)data, trakinfo.sequence_data_.size());
    if (ret < 0 || pps_len == 0 || sps_len == 0) {
        CSM_THROW_ERROR("get sps/pps error from extra data");
    }
    pps[0] = 0;
    pps[1] = 0;
    pps[2] = 0;
    pps[3] = 1;
    pps_len += 4;
    
    sps[0] = 0;
    sps[1] = 0;
    sps[2] = 0;
    sps[3] = 1;
    sps_len += 4;

    sendMediaPacket(MEDIA_VIDEO_TYPE, trakinfo.codec_type_,
        0, 0,
        false, true,
        sps, sps_len);
    sendMediaPacket(MEDIA_VIDEO_TYPE, trakinfo.codec_type_,
        0, 0,
        false, true,
        pps, pps_len);
}

void Mp4Demuxer::makeMovItems() {
    //handle sequnce data firstly
    for (const TrakInfo& trakinfo : mov_.traks_info_) {
        if (trakinfo.handler_type_ == "vide") {
            if (trakinfo.codec_type_ == MEDIA_CODEC_H264) {
                handleH264SpsPps(trakinfo);
            } else if (trakinfo.codec_type_ == MEDIA_CODEC_H265) {
                handleH265VpsSpsPps(trakinfo);
            } else {
                CSM_THROW_ERROR("not support video codec:%s", codectype_tostring(trakinfo.codec_type_).c_str());
            }
        } else if (trakinfo.handler_type_ == "soun") {
            if (trakinfo.codec_type_ == MEDIA_CODEC_AAC) {
                handleAACExtraData(trakinfo);
            } else {
                CSM_THROW_ERROR("not support audio codec:%s", codectype_tostring(trakinfo.codec_type_).c_str());
            }
        }
    }
    for (const TrakInfo& trakinfo : mov_.traks_info_) {
        size_t chunk_index = 1;
        uint32_t sample_index = 1;
        int64_t dts = 0;

        for (uint32_t chunk_offset : trakinfo.chunk_offsets_vec_) {
            for (size_t i = 0; i < trakinfo.chunk_sample_vec_.size(); i++) {
                bool found = false;
                uint32_t first_chunk = trakinfo.chunk_sample_vec_[i].first_chunk_;

                if (i == (trakinfo.chunk_sample_vec_.size() - 1)) {
                    found = chunk_index >= first_chunk;
                } else {
                    uint32_t next_first_chunk = trakinfo.chunk_sample_vec_[i + 1].first_chunk_;

                    found = (chunk_index >= first_chunk) && (chunk_index < next_first_chunk);
                }
                if (found) {
                    uint32_t samples_per_chunk = trakinfo.chunk_sample_vec_[i].samples_per_chunk_;

                    getSampleInfoInChunk(chunk_offset, chunk_index, samples_per_chunk, sample_index, dts, trakinfo);
                    sample_index += samples_per_chunk;
                }
            }
            chunk_index++;
        }
    }
}

void Mp4Demuxer::Output(Media_Packet_Ptr pkt_ptr) {
    if (options_["re"] == "true") {
        waiter_.Wait(pkt_ptr);
    }
    
    for(auto& sinker : sinkers_) {
        sinker.second->SourceData(pkt_ptr);
    }
}

void Mp4Demuxer::AddOption(const std::string& key, const std::string& value) {
    auto iter = options_.find(key);
    if (iter == options_.end()) {
        std::stringstream ss;
        ss << "the option key:" << key << " does not exist";
        throw CppStreamException(ss.str().c_str());
    }
    options_[key] = value;
    LogInfof(logger_, "set options key:%s, value:%s", key.c_str(), value.c_str());
}

void Mp4Demuxer::SetReporter(StreamerReport* reporter) {

}

}