#include "timesync.hpp"
#include "uuid.hpp"

void* make_timesync_streamer() {
    cpp_streamer::TimeSync* timesync = new cpp_streamer::TimeSync();

    return timesync;
}

void destroy_timesync_streamer(void* streamer) {
    cpp_streamer::TimeSync* timesync = (cpp_streamer::TimeSync*)streamer;

    delete timesync;
}

namespace cpp_streamer
{
#define TIME_SYNC_NAME "timesync"

TimeSync::TimeSync()
{
    name_ = TIME_SYNC_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
}

TimeSync::~TimeSync()
{
}

std::string TimeSync::StreamerName() {
    return name_;
}

void TimeSync::SetLogger(Logger* logger) {
    logger_ = logger;
}

int TimeSync::AddSinker(CppStreamerInterface* sinker) {
    if (!sinker) {
        return sinkers_.size();
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int TimeSync::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

int TimeSync::SourceData(Media_Packet_Ptr pkt_ptr) {
    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        HandleVideoPacket(pkt_ptr);
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        HandleAudioPacket(pkt_ptr);
    }
    return 0;
}

void TimeSync::OutputPacket(Media_Packet_Ptr pkt_ptr) {
    //LogInfof(logger_, "output packet:%s", pkt_ptr->Dump().c_str());
    for (auto sinker : sinkers_) {
        sinker.second->SourceData(pkt_ptr);
    }
}

void TimeSync::HandleVideoPacket(Media_Packet_Ptr pkt_ptr) {
    int64_t dts = pkt_ptr->dts_;
    int64_t pts = pkt_ptr->pts_;

    last_video_pkt_dts_ = dts;

    //first packet
    if (base_video_pkt_dts_ < 0) {
        base_video_pkt_dts_ = dts;
        base_video_pkt_pts_ = pts;

        //if audio has been inited.
        if ((base_audio_pkt_dts_ > 0) && (last_audio_pkt_dts_ > base_audio_pkt_dts_)) {
            int64_t diff_t = last_audio_pkt_dts_ - base_audio_pkt_dts_;
            base_video_pkt_dts_ -= diff_t;
            base_video_pkt_pts_ -= diff_t;
        }
        pkt_ptr->dts_ = dts - base_video_pkt_dts_;
        pkt_ptr->pts_ = pts - base_video_pkt_pts_;

        video_dts_ = pkt_ptr->dts_;
        video_pts_ = pkt_ptr->pts_;

        OutputPacket(pkt_ptr);
        return;
    }

    //if dts is reverse
    if (dts <= video_dts_) {
        //uint8_t* p = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
        LogInfof(logger_, "video packet new dts(%ld) reverse, old dts:%ld, len:%lu", 
                dts, video_dts_, pkt_ptr->buffer_ptr_->DataLen());
        //LogInfoData(logger_, p, 6, "video data");

        base_video_pkt_dts_ = dts - video_dts_;
        base_video_pkt_pts_ = pts - video_pts_;

        video_dts_ += 10;
        video_pts_ += 10;

        pkt_ptr->dts_ = video_dts_;
        pkt_ptr->pts_ = video_pts_;

        OutputPacket(pkt_ptr);
        return;
    }

    video_dts_ = dts - base_video_pkt_dts_;
    video_pts_ = pts - base_video_pkt_pts_ ;

    pkt_ptr->dts_ = video_dts_;
    pkt_ptr->pts_ = video_pts_;

    OutputPacket(pkt_ptr);
    return;
}

void TimeSync::HandleAudioPacket(Media_Packet_Ptr pkt_ptr) {
    int64_t dts = pkt_ptr->dts_;

    last_audio_pkt_dts_ = dts;

    if (base_audio_pkt_dts_ < 0) {
        base_audio_pkt_dts_ = dts;

        //if video has been inited.
        if ((base_video_pkt_dts_ > 0) && (last_video_pkt_dts_ > base_video_pkt_dts_)) {
            int64_t diff_t = last_video_pkt_dts_ - base_video_pkt_dts_;
            base_audio_pkt_dts_ -= diff_t;
        }
        pkt_ptr->dts_ = dts - base_audio_pkt_dts_;
        pkt_ptr->pts_ = dts - base_audio_pkt_dts_;

        audio_dts_ = pkt_ptr->dts_;

        OutputPacket(pkt_ptr);
        return;
    }

    //if dts is reverse
    if (dts <= audio_dts_) {
        LogInfof(logger_, "audio packet new dts(%ld) reverse, old dts:%ld, len:%lu", 
                dts, audio_dts_, pkt_ptr->buffer_ptr_->DataLen());

        base_audio_pkt_dts_ = dts - audio_dts_;

        audio_dts_ += 10;

        pkt_ptr->dts_ = audio_dts_;
        pkt_ptr->pts_ = audio_dts_;

        OutputPacket(pkt_ptr);
        return;
    }
    audio_dts_ = dts - base_audio_pkt_dts_;

    pkt_ptr->dts_ = audio_dts_;
    pkt_ptr->pts_ = audio_dts_;

    OutputPacket(pkt_ptr);
    return;
}

void TimeSync::StartNetwork(const std::string& url, void* loop_handle) {

}

void TimeSync::AddOption(const std::string& key, const std::string& value) {

}

void TimeSync::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}


}

