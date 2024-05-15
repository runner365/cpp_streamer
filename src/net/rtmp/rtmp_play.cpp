#include "rtmp_play.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "uuid.hpp"
#include "h264_h265_header.hpp"
#include <sstream>

void* make_rtmpplay_streamer() {
    cpp_streamer::RtmpPlay* player = new cpp_streamer::RtmpPlay();
    return player;
}

void destroy_rtmpplay_streamer(void* streamer) {
    cpp_streamer::RtmpPlay* player = (cpp_streamer::RtmpPlay*)streamer;
    delete player;
}

namespace cpp_streamer
{
#define RTMP_PLAY_NAME "rtmpplay"

RtmpPlay::RtmpPlay():statics_(MEDIA_STATICS_DEF_INTERVAL)
{
    name_ = RTMP_PLAY_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
}

RtmpPlay::~RtmpPlay()
{
    Release();
}

std::string RtmpPlay::StreamerName() {
    return name_;
}

void RtmpPlay::SetLogger(Logger* logger) {
    logger_ = logger;
}

int RtmpPlay::AddSinker(CppStreamerInterface* sinker) {
    if (!sinker) {
        return sinkers_.size();
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int RtmpPlay::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

int RtmpPlay::SourceData(Media_Packet_Ptr pkt_ptr) {
    return 0;
}

void RtmpPlay::ReportStatics() {
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

void RtmpPlay::OnMessage(int ret_code, Media_Packet_Ptr pkt_ptr) {
    int64_t now_ts = now_millisec();

    if (ret_code < 0) {
        return;
    }
    statics_.InputPacket(pkt_ptr);

    if ((now_ts - rpt_ts_) > 2000) {
        ReportStatics();
        rpt_ts_ = now_ts;
    }

    if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        pkt_ptr->buffer_ptr_->ConsumeData(2);
        pkt_ptr->fmt_type_ = MEDIA_FORMAT_RAW;
    } else if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        pkt_ptr->buffer_ptr_->ConsumeData(5);
        pkt_ptr->fmt_type_ = MEDIA_FORMAT_RAW;

        uint8_t* extra_data = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
        size_t extra_len = pkt_ptr->buffer_ptr_->DataLen();

        if (pkt_ptr->is_seq_hdr_) {
            int ret = GetSpsPpsFromExtraData(pps_, pps_len_,
                           sps_, sps_len_, 
                           extra_data, extra_len);
            if (ret < 0 || sps_len_ == 0 || pps_len_ == 0) {
                LogErrorf(logger_, "fail to get sps/pps from video sequence header, packet dump:%s",
                    pkt_ptr->Dump(true).c_str());
                return;
            }
            Media_Packet_Ptr sps_ptr = std::make_shared<Media_Packet>(sps_len_);
            sps_ptr->copy_properties(pkt_ptr);
            sps_ptr->buffer_ptr_->AppendData((char*)H264_START_CODE, sizeof(H264_START_CODE));
            sps_ptr->buffer_ptr_->AppendData((char*)sps_, sps_len_);

            Media_Packet_Ptr pps_ptr = std::make_shared<Media_Packet>(pps_len_);
            pps_ptr->copy_properties(pkt_ptr);
            pps_ptr->buffer_ptr_->AppendData((char*)H264_START_CODE, sizeof(H264_START_CODE));
            pps_ptr->buffer_ptr_->AppendData((char*)pps_, pps_len_);

            for (auto sinker : sinkers_) {
                LogInfof(logger_, "sps packet:%s", sps_ptr->Dump(true).c_str());
                LogInfof(logger_, "pps packet:%s", pps_ptr->Dump(true).c_str());
                sinker.second->SourceData(sps_ptr);
                sinker.second->SourceData(pps_ptr);
            }
            return;
        }
        std::vector<std::shared_ptr<DataBuffer>> nalus;
        bool ret = cpp_streamer::Avcc2Nalus((uint8_t*)pkt_ptr->buffer_ptr_->Data(),
            pkt_ptr->buffer_ptr_->DataLen(),
            nalus);

        if (!ret || nalus.empty()) {
            return;
        }
        for (std::shared_ptr<DataBuffer> db_ptr : nalus) {
            Media_Packet_Ptr nalu_ptr = std::make_shared<Media_Packet>(db_ptr->DataLen());

            nalu_ptr->copy_properties(pkt_ptr);
            nalu_ptr->buffer_ptr_->AppendData(db_ptr->Data(), db_ptr->DataLen());
            for (auto sinker : sinkers_) {
                sinker.second->SourceData(nalu_ptr);
            }
            return;
        }
    } else if (pkt_ptr->av_type_ == MEDIA_METADATA_TYPE){
        pkt_ptr->fmt_type_ = MEDIA_FORMAT_RAW;

        LogInfof(logger_, "meta data dump:%s", pkt_ptr->Dump().c_str());
        ReportMetaData((uint8_t*)pkt_ptr->buffer_ptr_->Data(),
            pkt_ptr->buffer_ptr_->DataLen());
        for (auto sinker : sinkers_) {
            sinker.second->SourceData(pkt_ptr);
        }
        return;
    } else {
        LogErrorf(logger_, "rtmp play get unkown av type:%d", pkt_ptr->av_type_);
    }

    for (auto sinker : sinkers_) {
        sinker.second->SourceData(pkt_ptr);
    }
}

void RtmpPlay::ReportMetaData(uint8_t* data, size_t len) {
    uint8_t* p = data;
    int left_len = (int)len;
    std::stringstream ss;
    bool is_key = true;

    ss << "{";
    while (left_len > 0) {
        AMF_ITERM amf_item;

        AMF_Decoder::Decode(p, left_len, amf_item);

        if (amf_item.amf_type_ == AMF_DATA_TYPE_STRING
            || amf_item.amf_type_ == AMF_DATA_TYPE_LONG_STRING) {
            if (is_key) {
                is_key = false;
                ss << amf_item.DumpAmf() << ":";
            } else {
                is_key = true;
                ss << amf_item.DumpAmf();
            }
        } else {
            is_key = true;
            ss << amf_item.DumpAmf();
        }

        if (left_len > 0 && is_key) {
            uint8_t next_amf_type = *p;
            if (next_amf_type != (uint8_t)AMF_DATA_TYPE_UNKNOWN
                && next_amf_type != (uint8_t)AMF_DATA_TYPE_OBJECT_END) {
                ss << ",";
            }
        }
    }
    ss << "}";
    ReportEvent("MetaData", ss.str());
}

void RtmpPlay::OnRtmpHandShakeSendC0C1(int ret_code, uint8_t* data, size_t len) {
    std::string desc = Data2HexString(data, len);

    ReportEvent("SendC0C1", desc);
}

void RtmpPlay::OnRtmpHandShakeRecvS0S1S2(int ret_code, uint8_t* data, size_t len) {
    std::string desc = Data2HexString(data, len);
    ReportEvent("RecvS0S1S2", desc);
}

void RtmpPlay::OnRtmpConnectSend(int ret_code, const std::map<std::string, std::string>& items) {
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

void RtmpPlay::OnRtmpConnectRecv(
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
        ss << "\"objs\":";
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

void RtmpPlay::OnRtmpChunkSize(
        int ret,
        uint32_t chunk_size) {
    if (ret < 0) {
        ReportEvent("ChunkSize", "error");
        return;
    }

    ReportEvent("ChunkSize", std::to_string(chunk_size));
}

void RtmpPlay::OnRtmpWindowSize(
        int ret,
        uint32_t window_size) {
    if (ret < 0) {
        ReportEvent("WindowSize", "error");
        return;
    }
    ReportEvent("WindowSize", std::to_string(window_size));
}

void RtmpPlay::OnRtmpBandWidth(
        int ret,
        uint32_t bandwidth) {
    if (ret < 0) {
        ReportEvent("BandWidth", "error");
        return;
    }
    ReportEvent("BandWidth", std::to_string(bandwidth));
}

void RtmpPlay::OnRtmpCtrlAck() {
    ReportEvent("CtrlAck", "ok");
}

void RtmpPlay::OnRtmpCreateStreamSend(
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

void RtmpPlay::OnRtmpCreateStreamRecv(
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

void RtmpPlay::OnRtmpPlayPublishSend(
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

void RtmpPlay::OnRtmpPlayPublishRecv(
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
    ReportEvent("PlayPublishRecv", ss.str());
}
void RtmpPlay::OnClose(int ret_code) {
    ReportEvent("close", "");
}

void RtmpPlay::OnWork() {
    loop_ = (uv_loop_t*)malloc(sizeof(uv_loop_t));
    uv_loop_init(loop_);

    Init();

    while(running_) {
        uv_run(loop_, UV_RUN_DEFAULT);
    }
    if (client_session_) {
        client_session_->Close();
        client_session_ = nullptr;
    }
}

void RtmpPlay::Init() {
    LogInfof(logger_, "rtmp play init, src url:%s", src_url_.c_str());
    client_session_ = new RtmpClientSession(loop_, this, this, logger_);
    client_session_->Start(src_url_, false);
}

void RtmpPlay::Release() {
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

void RtmpPlay::ReportEvent(const std::string& type, const std::string& value) {
    if (report_) {
        report_->OnReport(name_, type, value);
    }
}

void RtmpPlay::StartNetwork(const std::string& url, void* loop_handle) {
    src_url_ = url;
    if (!loop_handle) {
        running_ = true;
        thread_ptr_ = std::make_shared<std::thread>(&RtmpPlay::OnWork, this);
    } else {
        loop_ = (uv_loop_t*)loop_handle;
        Init();
    }

}

void RtmpPlay::AddOption(const std::string& key, const std::string& value) {

}

void RtmpPlay::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}

}
