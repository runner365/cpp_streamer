#include "rtmp_play.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "uuid.hpp"
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

    LogDebugf(logger_, "rtmp play receive ret_code:%d, packet:%s", ret_code, pkt_ptr->Dump().c_str());

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
    } else if (pkt_ptr->av_type_ == MEDIA_METADATA_TYPE){
        pkt_ptr->fmt_type_ = MEDIA_FORMAT_RAW;
        return;
    } else {
        LogErrorf(logger_, "rtmp play get unkown av type:%d", pkt_ptr->av_type_);
    }

    for (auto sinker : sinkers_) {
        sinker.second->SourceData(pkt_ptr);
    }
}

void RtmpPlay::OnRtmpHandShake(int ret_code) {
    ReportEvent("event", "handshake");
}

void RtmpPlay::OnRtmpConnect(int ret_code) {
    ReportEvent("event", "rtmpconnect");
}

void RtmpPlay::OnRtmpCreateStream(int ret_code) {
    ReportEvent("event", "createstream");
}

void RtmpPlay::OnRtmpPlayPublish(int ret_code) {
    ReportEvent("event", "play");
}

void RtmpPlay::OnClose(int ret_code) {
    ReportEvent("event", "close");
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
    client_session_ = new RtmpClientSession(loop_, this, logger_);
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
