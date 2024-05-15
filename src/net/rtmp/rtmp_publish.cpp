#include "rtmp_publish.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "uuid.hpp"
#include "flv_pub.hpp"
#include "h264_h265_header.hpp"

#include <sstream>

void* make_rtmppublish_streamer() {
    cpp_streamer::RtmpPublish* publisher = new cpp_streamer::RtmpPublish();
    return publisher;
}

void destroy_rtmppublish_streamer(void* streamer) {
    cpp_streamer::RtmpPublish* publisher = (cpp_streamer::RtmpPublish*)streamer;
    delete publisher;
}

namespace cpp_streamer
{

#define RTMP_PUBLISH_NAME "rtmppublish"

void SourceRtmpData(uv_async_t *handle) {
    RtmpPublish* push = (RtmpPublish*)(handle->data);
    push->HandleMediaData();
}

RtmpPublish::RtmpPublish():statics_(MEDIA_STATICS_DEF_INTERVAL)

{
    name_ = RTMP_PUBLISH_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
}

RtmpPublish::~RtmpPublish()
{
    Release();
}

Media_Packet_Ptr RtmpPublish::GetMediaPacket() {
    std::lock_guard<std::mutex> lock(mutex_);

    Media_Packet_Ptr pkt_ptr;
    if (packet_queue_.empty()) {
        return pkt_ptr;
    }
        
    pkt_ptr = packet_queue_.front();
    packet_queue_.pop();

    return pkt_ptr;
}

void RtmpPublish::HandleVideoData(Media_Packet_Ptr pkt_ptr) {
    uint8_t* data = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
    int data_len = pkt_ptr->buffer_ptr_->DataLen();

    if (data_len < 5) {
        LogErrorf(logger_, "rtmp handle video data len:%d error", data_len);
        ReportEvent("error", "video data length error");
        return;
    }
   
    if (pkt_ptr->is_seq_hdr_ && pkt_ptr->fmt_type_ == MEDIA_FORMAT_FLV) {
        SendVideo(pkt_ptr);
        return;
    }
    std::vector<std::shared_ptr<DataBuffer>> nalus;
    bool ret = AnnexB2Nalus(data, data_len, nalus);
    if (!ret) {
        LogErrorf(logger_, "flv mux input nalu data error:%d", data_len);
        ReportEvent("error", "video data nalu error");
        return;
    }

    for (auto& nalu : nalus) {
        uint8_t* p = (uint8_t*)nalu->Data();
        size_t len = nalu->DataLen();

        int nalu_type_pos = GetNaluTypePos(p);
        if (nalu_type_pos < 3) {
            LogErrorf(logger_, "flv mux input data fail to find nalu start code");
            continue;
        }
        if (H264_IS_SPS(p[nalu_type_pos])) {
            memcpy(sps_, p + nalu_type_pos, len - nalu_type_pos);
            sps_len_ = len - nalu_type_pos;
            LogInfoData(logger_, sps_, sps_len_, "sps data");
            continue;
        }
        if (H264_IS_PPS(p[nalu_type_pos])) {
            memcpy(pps_, p + nalu_type_pos, len - nalu_type_pos);
            pps_len_ = len - nalu_type_pos;
            LogInfoData(logger_, pps_, pps_len_, "pps data");
            continue;
        }
        if (H264_IS_AUD(p[nalu_type_pos])) {
            continue;
        }

        if (pps_len_ < 0 || sps_len_ < 0) {
            continue;
        }
        if (H264_IS_KEYFRAME(p[nalu_type_pos]) || !first_video_) {
            uint8_t extra_data[1024];
            int extra_len = 0;

            if (!first_video_) {
                LogInfoData(logger_, extra_data, extra_len, "Avcc header");
            }
            first_video_ = true;

            get_video_extradata(pps_, pps_len_, sps_, sps_len_,
                    extra_data, extra_len);

            Media_Packet_Ptr seq_ptr = std::make_shared<Media_Packet>();
            seq_ptr->copy_properties(*(pkt_ptr.get()));
            seq_ptr->is_seq_hdr_ = true;
            seq_ptr->is_key_frame_ = false;
            seq_ptr->buffer_ptr_->Reset();
            seq_ptr->buffer_ptr_->AppendData((char*)extra_data, extra_len);
            SendVideo(seq_ptr);
        }

        int nalu_len = len - nalu_type_pos;
        
        uint8_t nalu_len_data[4];

        ByteStream::Write4Bytes(nalu_len_data, nalu_len);
        p += nalu_type_pos;

        Media_Packet_Ptr video_ptr = std::make_shared<Media_Packet>();
        video_ptr->copy_properties(*(pkt_ptr.get()));
        video_ptr->is_seq_hdr_   = false;
        video_ptr->is_key_frame_ = H264_IS_KEYFRAME(p[nalu_type_pos]);
        video_ptr->buffer_ptr_->Reset();
        video_ptr->buffer_ptr_->AppendData((char*)nalu_len_data, sizeof(nalu_len_data));
        video_ptr->buffer_ptr_->AppendData((char*)p, nalu_len);

        SendVideo(video_ptr);
    }
}

void RtmpPublish::SendVideo(Media_Packet_Ptr pkt_ptr) {
    uint8_t* p = (uint8_t*)pkt_ptr->buffer_ptr_->ConsumeData(-5);
    
    p[0] = 0;
    if (pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
        p[0] |= FLV_VIDEO_H264_CODEC;
    } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_H265) {
        p[0] |= FLV_VIDEO_H265_CODEC;
    } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP8) {
        p[0] |= FLV_VIDEO_VP8_CODEC;
    } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP9) {
        p[0] |= FLV_VIDEO_VP9_CODEC;
    }  else {
        LogErrorf(logger_, "unsuport video codec type:%d", pkt_ptr->codec_type_);
        return;
    }
    
    if (pkt_ptr->is_key_frame_ || pkt_ptr->is_seq_hdr_) {
        p[0] |= FLV_VIDEO_KEY_FLAG;
        if (pkt_ptr->is_seq_hdr_) {
            p[1] = 0x00;
        } else {
            p[1] = 0x01;
        }
    } else {
        p[0] |= FLV_VIDEO_INTER_FLAG;
        p[1] = 0x01;
    }
    uint32_t ts_delta = (pkt_ptr->pts_ > pkt_ptr->dts_) ? (pkt_ptr->pts_ - pkt_ptr->dts_) : 0;
    p[2] = (ts_delta >> 16) & 0xff;
    p[3] = (ts_delta >> 8) & 0xff;
    p[4] = ts_delta & 0xff;

    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    SendRtmp(pkt_ptr);
}

void RtmpPublish::HandleAudioData(Media_Packet_Ptr pkt_ptr) {
    uint8_t* p;

    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        p = (uint8_t*)pkt_ptr->buffer_ptr_->ConsumeData(-2);

        if (pkt_ptr->codec_type_ == MEDIA_CODEC_AAC) {
            p[0] = FLV_AUDIO_AAC_CODEC | 0x0f;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_OPUS) {
            p[0] = FLV_AUDIO_OPUS_CODEC | 0x0f;
        } else {
            LogErrorf(logger_, "unsuport audio codec type:%d", pkt_ptr->codec_type_);
            return;
        }

        if (pkt_ptr->is_seq_hdr_) {
            p[1] = 0x00;
        } else {
            p[1] = 0x01;
        }
    }

    SendRtmp(pkt_ptr);
    return;


}

void RtmpPublish::HandleMediaData() {
    while(true) {
        Media_Packet_Ptr pkt_ptr = GetMediaPacket();
        if (!pkt_ptr) {
            break;
        }
        
        if (pkt_ptr->fmt_type_ == MEDIA_FORMAT_FLV) {
            SendRtmp(pkt_ptr);
            return;
        }

        if (pkt_ptr->fmt_type_ == MEDIA_FORMAT_RAW) {
            if(pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
                HandleVideoData(pkt_ptr);
            } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
                HandleAudioData(pkt_ptr);
            } else {
                LogErrorf(logger_, "not suport av type:%s",
                        avtype_tostring(pkt_ptr->av_type_).c_str());
            }
        } else {
            LogErrorf(logger_, "not suport format:%d", pkt_ptr->fmt_type_);
        }

    }
    return;
}


void RtmpPublish::SendRtmp(Media_Packet_Ptr pkt_ptr) {
    if (pkt_ptr->fmt_type_ != MEDIA_FORMAT_FLV) {
        LogErrorf(logger_, "media packet must be flv type, packet:%s", pkt_ptr->Dump().c_str());
        return;
    }

    if (!client_session_ || !client_session_->IsReady() || !ready_) {
        LogErrorf(logger_, "rtmp session is not ready");
        return;
    }
    int64_t now_ts = now_millisec();

    statics_.InputPacket(pkt_ptr);

    if ((now_ts - rpt_ts_) > 2000) {
        ReportStatics();
        rpt_ts_ = now_ts;
    }

    // LogInfof(logger_, "rtmp publish output packet:%s", pkt_ptr->Dump(true).c_str());
    client_session_->RtmpWrite(pkt_ptr);
}

std::string RtmpPublish::StreamerName() {
    return name_;
}

void RtmpPublish::SetLogger(Logger* logger) {
    logger_ = logger;
}

int RtmpPublish::AddSinker(CppStreamerInterface* sinker) {
    if (!sinker) {
        return sinkers_.size();
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int RtmpPublish::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

int RtmpPublish::SourceData(Media_Packet_Ptr pkt_ptr) {
    std::lock_guard<std::mutex> lock(mutex_);

    // LogInfof(logger_, "rtmppublish input packet:%s", pkt_ptr->Dump(true).c_str());
    packet_queue_.push(pkt_ptr);

    async_.data = (void*)this;
    uv_async_send(&async_);
    return (int)packet_queue_.size();
}

void RtmpPublish::StartNetwork(const std::string& url, void* loop_handle) {
    src_url_ = url;
    if (!loop_handle) {
        running_ = true;
        thread_ptr_ = std::make_shared<std::thread>(&RtmpPublish::OnWork, this);
    } else {
        loop_ = (uv_loop_t*)loop_handle;
        uv_async_init(loop_, &async_, SourceRtmpData);
        Init();
    }
}

void RtmpPublish::AddOption(const std::string& key, const std::string& value) {

}

void RtmpPublish::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}

void RtmpPublish::ReportEvent(const std::string& type, const std::string& value) {
    if (report_) {
        report_->OnReport(name_, type, value);
    }
}

void RtmpPublish::OnMessage(int ret_code, Media_Packet_Ptr pkt_ptr) {
    LogErrorf(logger_, "rtmp publish should not receive media packet:%s", pkt_ptr->Dump().c_str());
}


void RtmpPublish::OnRtmpHandShakeSendC0C1(int ret_code, uint8_t* data, size_t len) {
    std::string desc = Data2HexString(data, len);

    ReportEvent("SendC0C1", desc);
}

void RtmpPublish::OnRtmpHandShakeRecvS0S1S2(int ret_code, uint8_t* data, size_t len) {
    std::string desc = Data2HexString(data, len);
    ReportEvent("RecvS0S1S2", desc);
}

void RtmpPublish::OnRtmpConnectSend(int ret_code, const std::map<std::string, std::string>& items) {
    if (ret_code < 0) {
        ReportEvent("RtmpConnectSend", "error");
        return;
    }
    std::stringstream ss;
    int index = 0;
    
    ss << "[";
    for (auto item : items) {
        ss << "{";
        ss << "\"" << item.first << "\":";
        ss << "\"" << item.second << "\"";
        ss << "}";
        index++;
        if (index < items.size()) {
            ss << ",";
        }
    }
    ss << "]";
    ReportEvent("RtmpConnectSend", ss.str());
}

void RtmpPublish::OnRtmpConnectRecv(
        int ret,
        const std::string& result,
        int64_t transaction_id,
        const std::map<std::string, std::string>& items) {
    if (ret < 0) {
        ReportEvent("RtmpConnectRecv", "error");
        return;
    }
    std::stringstream ss;
    int index = 0;

    ss << "{";
    ss << "\"result\":" << "\"" << result << "\"";
    ss << ",";
    ss << "\"transactionId\":" << transaction_id;
    if (items.size() > 0) {
        ss << ",";
        ss << "[";
        for (auto item : items) {
            ss << "{";
            ss << "\"" << item.first << "\":";
            ss << "\"" << item.second << "\"";
            ss << "}";
            index++;
            if (index < items.size()) {
                ss << ",";
            }
        }
        ss << "]";
    }
    ss << "}";
    ReportEvent("RtmpConnectRecv", ss.str());
}

void RtmpPublish::OnRtmpChunkSize(
        int ret,
        uint32_t chunk_size) {
    if (ret < 0) {
        ReportEvent("ChunkSize", "error");
        return;
    }

    ReportEvent("ChunkSize", std::to_string(chunk_size));
}

void RtmpPublish::OnRtmpWindowSize(
        int ret,
        uint32_t window_size) {
    if (ret < 0) {
        ReportEvent("WindowSize", "error");
        return;
    }
    ReportEvent("WindowSize", std::to_string(window_size));
}

void RtmpPublish::OnRtmpBandWidth(
        int ret,
        uint32_t bandwidth) {
    if (ret < 0) {
        ReportEvent("BandWidth", "error");
        return;
    }
    ReportEvent("BandWidth", std::to_string(bandwidth));
}

void RtmpPublish::OnRtmpCtrlAck() {
    ReportEvent("CtrlAck", "ok");
}

void RtmpPublish::OnRtmpCreateStreamSend(
        int ret,
        int64_t transaction_id) {
    if (ret < 0) {
        ReportEvent("CreateStreamSend", "error");
        return;
    }
    std::stringstream ss;

    ss << "{";
    ss << "\"transactionId\":" << transaction_id;
    ss << "}";
    ReportEvent("CreateStreamSend", ss.str());
}

void RtmpPublish::OnRtmpCreateStreamRecv(
        int ret,
        const std::string& result,
        int64_t transaction_id,
        int64_t stream_id,
        const std::map<std::string, std::string>& items) {
    if (ret < 0) {
        ReportEvent("CreateStreamRecv", "error");
        return;
    }
    std::stringstream ss;
    int index = 0;

    ss << "{";
    ss << "\"result\":" << "\"" << result << "\"";
    ss << ",";
    ss << "\"transactionId\":" << transaction_id;
    ss << ",";
    ss << "\"streamId\":" << stream_id;
    if (items.size() > 0) {
        ss << ",";
        ss << "[";
        for (auto item : items) {
            ss << "{";
            ss << "\"" << item.first << "\":";
            ss << "\"" << item.second << "\"";
            ss << "}";
            index++;
            if (index < items.size()) {
                ss << ",";
            }
        }
        ss << "]";
    }
    ss << "}";
    ReportEvent("CreateStreamRecv", ss.str());
}

void RtmpPublish::OnRtmpPlayPublishSend(
        const std::string& oper,//play or publish
        int64_t transaction_id,
        const std::string stream_name) {
    std::stringstream ss;

    ss << "{";
    ss << "\"oper\":" << "\"" << oper << "\"";
    ss << ",";
    ss << "\"transactionId\":" << transaction_id;
    ss << ",";
    ss << "\"streamname\":" << "\"" << stream_name << "\"";
    ss << "}";
    ReportEvent("PlayPublishSend", ss.str());
}

void RtmpPublish::OnRtmpPlayPublishRecv(
        int ret,
        const std::string& status,
        int64_t transaction_id,
        const std::map<std::string, std::string>& items) {
    std::stringstream ss;
    int index = 0;

    if (ret < 0) {
        ReportEvent("PlayPublishRecv", "error");
        return;
    }
    ss << "{";
    ss << "\"status\":" << "\"" << status << "\"";
    ss << ",";
    ss << "\"transactionId\":" << transaction_id;
    if (items.size() > 0) {
        ss << ",";
        ss << "\"objs\":[";
        for (auto item : items) {
            ss << "{";
            ss << "\"" << item.first << "\":";
            ss << "\"" << item.second << "\"";
            ss << "}";
            index++;
            if (index < items.size()) {
                ss << ",";
            }
        }
        ss << "]";
    }
    
    ss << "}";

    ready_ = (status == "_result");
    ReportEvent("PlayPublishRecv", ss.str());
}

void RtmpPublish::OnClose(int ret_code) {
    ReportEvent("close", "");
}

void RtmpPublish::OnWork() {
    loop_ = (uv_loop_t*)malloc(sizeof(uv_loop_t));
    uv_loop_init(loop_);
    uv_async_init(loop_, &async_, SourceRtmpData);
    Init();

    while(running_) {
        uv_run(loop_, UV_RUN_DEFAULT);
    }
    if (client_session_) {
        client_session_->Close();
        client_session_ = nullptr;
    }
}

void RtmpPublish::Init() {
    LogInfof(logger_, "rtmp publish init, src url:%s", src_url_.c_str());
    client_session_ = new RtmpClientSession(loop_, this, this, logger_);
    client_session_->Start(src_url_, true);
}

void RtmpPublish::Release() {
    ready_ = false;
    if (running_ && (thread_ptr_ != nullptr) && loop_ != nullptr) {
        running_ = false;
        uv_loop_close(loop_);
        thread_ptr_->join();
        thread_ptr_ = nullptr;
        loop_ = nullptr;
    } else {
        if (client_session_) {
            client_session_->Close();
            client_session_ = nullptr;
        }
    }
}

void RtmpPublish::ReportStatics() {
    std::stringstream ss;

    ss << "{";
    ss << "\"vkbits\":" << statics_.GetVideoKbitRate() << ",";
    ss << "\"vframes\":" << statics_.GetVideoFrameRate() << ",";

    ss << "\"akbits\":" << statics_.GetAudioKbitRate() << ",";
    ss << "\"aframes\":" << statics_.GetAudioFrameRate() << ",";
    ss << "\"gop\":" << statics_.GetGop();
    ss << "}";

    ReportEvent("statics", ss.str());
}

}
