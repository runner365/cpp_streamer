#include "rtmp_session_base.hpp"
#include "rtmp_pub.hpp"
#include "flv_pub.hpp"
#include <map>

namespace cpp_streamer
{

const char* server_phase_desc_list[] = {"handshake c2 phase",
                                        "connect phase",
                                        "create stream phase",
                                        "create publish/play phase",
                                        "media handle phase"};

const char* client_phase_desc_list[] = {
    "c0c1 phase", // 0
    "s0s1s2 phase", // 1
    "connect phase", // 2
    "connect response phase",// 3
    "create stream phase",// 4
    "create stream response phase", // 5
    "create play phase", // 6
    "create publish phase", // 7
    "create play publish response phase" // 8
    "media handle phase" // 9
    };

const char* GetServerPhaseDesc(RTMP_SERVER_SESSION_PHASE phase) {
    return server_phase_desc_list[phase];
}

const char* GetClientPhaseDesc(RTMP_CLIENT_SESSION_PHASE phase) {
    return client_phase_desc_list[phase];
}

RtmpSessionBase::RtmpSessionBase(Logger* logger):recv_buffer_(50*1024)
                                                 , logger_(logger)
{
}

RtmpSessionBase::~RtmpSessionBase()
{
}

int RtmpSessionBase::ReadFmtCsid() {
    uint8_t* p = nullptr;

    if (recv_buffer_.Require(1)) {
        p = (uint8_t*)recv_buffer_.Data();
        fmt_  = ((*p) >> 6) & 0x3;
        csid_ = (*p) & 0x3f;
        recv_buffer_.ConsumeData(1);
    } else {
        return RTMP_NEED_READ_MORE;
    }

    if (csid_ == 0) {
        if (recv_buffer_.Require(1)) {//need 1 byte
            p = (uint8_t*)recv_buffer_.Data();
            recv_buffer_.ConsumeData(1);
            csid_ = 64 + *p;
        } else {
            return RTMP_NEED_READ_MORE;
        }
    } else if (csid_ == 1) {
        if (recv_buffer_.Require(2)) {//need 2 bytes
            p = (uint8_t*)recv_buffer_.Data();
            recv_buffer_.ConsumeData(2);
            csid_ = 64;
            csid_ += *p++;
            csid_ += *p;
        } else {
            return RTMP_NEED_READ_MORE;
        }
    } else {
        //normal csid: 2~64
    }

    return RTMP_OK;
}

void RtmpSessionBase::SetChunkSize(uint32_t chunk_size) {
    chunk_size_ = chunk_size;
}

uint32_t RtmpSessionBase::GetChunkSize() {
    return chunk_size_;
}

bool RtmpSessionBase::IsPublish() {
    return req_.publish_flag_;
}

const char* RtmpSessionBase::IsPublishDesc() {
    return req_.publish_flag_ ? "publish" : "play";
}


Media_Packet_Ptr RtmpSessionBase::GetMediaPacket(CHUNK_STREAM_PTR cs_ptr) {
    Media_Packet_Ptr pkt_ptr;
    uint32_t ts_delta = 0;

    if (cs_ptr->chunk_data_ptr_->DataLen() < 2) {
        LogErrorf(logger_, "rtmp chunk media size:%lu is too small", cs_ptr->chunk_data_ptr_->DataLen());
        return pkt_ptr;
    }
    uint8_t* p = (uint8_t*)cs_ptr->chunk_data_ptr_->Data();

    pkt_ptr = std::make_shared<Media_Packet>();

    pkt_ptr->typeid_   = cs_ptr->type_id_;
    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;

    if (cs_ptr->type_id_ == RTMP_MEDIA_PACKET_VIDEO) {
        uint8_t codec = p[0] & 0x0f;

        pkt_ptr->av_type_ = MEDIA_VIDEO_TYPE;
        if (codec == FLV_VIDEO_H264_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_H264;
        } else if (codec == FLV_VIDEO_H265_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_H265;
        } else if (codec == FLV_VIDEO_VP8_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_VP8;
        } else if (codec == FLV_VIDEO_VP9_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_VP9;
        }  else {
            LogErrorf(logger_, "does not support video codec typeid:%d, 0x%02x", cs_ptr->type_id_, p[0]);
            //assert(0);
            return pkt_ptr;
        }

        uint8_t frame_type = p[0] & 0xf0;
        uint8_t nalu_type = p[1];
        if (frame_type == FLV_VIDEO_KEY_FLAG) {
            if (nalu_type == FLV_VIDEO_AVC_SEQHDR) {
                pkt_ptr->is_seq_hdr_ = true;
            } else if (nalu_type == FLV_VIDEO_AVC_NALU) {
                pkt_ptr->is_key_frame_ = true;
            } else {
                LogErrorf(logger_, "input flv video error, 0x%02x 0x%02x", p[0], p[1]);
                return pkt_ptr;
            }
        } else if (frame_type == FLV_VIDEO_INTER_FLAG) {
            pkt_ptr->is_key_frame_ = false;
        }

        ts_delta = ByteStream::Read3Bytes(p + 2);
    } else if (cs_ptr->type_id_ == RTMP_MEDIA_PACKET_AUDIO) {
        pkt_ptr->av_type_ = MEDIA_AUDIO_TYPE;
        uint8_t frame_type = p[0] & 0xf0;

        if (frame_type == FLV_AUDIO_AAC_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_AAC;
            if(p[1] == 0x00) {
                pkt_ptr->is_seq_hdr_ = true;
            } else if (p[1] == 0x01) {
                pkt_ptr->is_key_frame_ = false;
                pkt_ptr->is_seq_hdr_   = false;
            }
        } else if (frame_type == FLV_AUDIO_OPUS_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_OPUS;
            if(p[1] == 0x00) {
                pkt_ptr->is_seq_hdr_ = true;
            } else if (p[1] == 0x01) {
                pkt_ptr->is_key_frame_ = false;
                pkt_ptr->is_seq_hdr_   = false;
            }
        } else {
            LogErrorf(logger_, "does not support audio codec typeid:%d, 0x%02x", cs_ptr->type_id_, p[0]);
            assert(0);
            return pkt_ptr;
        }
    } else if ((cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA0) || (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA3)) {
        pkt_ptr->av_type_ = MEDIA_METADATA_TYPE;
    } else {
        LogWarnf(logger_, "rtmp input unkown media type:%d", cs_ptr->type_id_);
        assert(0);
        return pkt_ptr;
    }

    if (ts_delta > 500) {
        LogWarnf(logger_, "video ts_delta error:%u", ts_delta);
    }
    pkt_ptr->dts_  = cs_ptr->timestamp32_;
    pkt_ptr->pts_  = pkt_ptr->dts_ + ts_delta;
    pkt_ptr->buffer_ptr_->Reset();
    pkt_ptr->buffer_ptr_->AppendData(cs_ptr->chunk_data_ptr_->Data(), cs_ptr->chunk_data_ptr_->DataLen());

    pkt_ptr->app_        = req_.app_;
    pkt_ptr->streamname_ = req_.stream_name_;
    pkt_ptr->key_        = req_.key_;
    pkt_ptr->streamid_   = cs_ptr->msg_stream_id_;

    return pkt_ptr;
}

int RtmpSessionBase::ReadChunkStream(CHUNK_STREAM_PTR& cs_ptr) {
    int ret = -1;

    if (!fmt_ready_) {
        ret = ReadFmtCsid();
        if (ret != 0) {
            return ret;
        }
        fmt_ready_ = true;
    }

    std::map<uint8_t, CHUNK_STREAM_PTR>::iterator iter = cs_map_.find(csid_);
    if (iter == cs_map_.end()) {
        cs_ptr = std::make_shared<ChunkStream>(this, fmt_, csid_, chunk_size_, logger_);
        cs_map_.insert(std::make_pair(csid_, cs_ptr));
    } else {
        cs_ptr =iter->second;
        cs_ptr->chunk_size_ = chunk_size_;
    }

    ret = cs_ptr->ReadMessageHeader(fmt_, csid_);
    if ((ret < RTMP_OK) || (ret == RTMP_NEED_READ_MORE)) {
        return ret;
    } else {
        ret = cs_ptr->ReadMessagePayload();
        //cs_ptr->DumpHeader();
        if (ret == RTMP_OK) {
            fmt_ready_ = false;
            //cs_ptr->DumpPayload();
            return ret;
        }
    }

    return ret;
}

}
