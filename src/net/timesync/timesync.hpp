#ifndef TIME_SYNC_HPP
#define TIME_SYNC_HPP

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

TimerSync::TimeSync()
{
    name_ = TIME_SYNC_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
}

TimerSync::~TimeSync()
{
}

std::string TimerSync::StreamerName() {
    return name_;
}

void TimerSync::SetLogger(Logger* logger) {
    logger_ = logger;
}

int TimerSync::AddSinker(CppStreamerInterface* sinker) {
    if (!sinker) {
        return sinkers_.size();
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int TimerSync::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

int TimerSync::SourceData(Media_Packet_Ptr pkt_ptr) {
    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        HandleVideoPacket(pkt_ptr);
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        HandleAudioPacket(pkt_ptr);
    }
}

void TimerSync::OutputPacket(Media_Packet_Ptr pkt_ptr) {
    for (auto sinker : sinkers_) {
        sinker.second->SourceData(pkt_ptr);
    }
}

void TimerSync::HandleVideoPacket(Media_Packet_Ptr pkt_ptr) {
    int64_t dts = pkt_ptr->dts_;
    int64_t pts = pkt_ptr->pts_;

    if (last_video_dts_ < 0 || last_video_pts_ < 0) {
        last_video_dts_ = dts;
        last_video_pts_ = pts;

        video_dts_ = 0;
        video_pts_ = 0;
        pkt_ptr->dts_ = video_dts_;
        pkt_ptr->pts_ = video_pts_;
        OutputPacket(pkt_ptr);
        return;
    }

    //if dts is reverse
    if (dts <= last_video_dts_) {
        LogInfof(logger_, "video packet new dts(%ld) reverse, old dts:%ld", 
                dts, last_video_dts_);

        video_dts_ += 1;
        video_pts_ += 1;
        pkt_ptr->dts_ = video_dts_;
        pkt_ptr->pts_ = video_pts_;

        last_video_dts_ = dts;
        last_video_pts_ = pts;

        OutputPacket(pkt_ptr);
        return;
    }

    int64_t diff_dts = dts - last_video_dts_;
    int64_t diff_pts = pts - last_video_pts_;

    video_dts_ += diff_dts;
    video_pts_ += diff_pts;

    pkt_ptr->dts_ = video_dts_;
    pkt_ptr->pts_ = video_pts_;

    last_video_dts_ = dts;
    last_video_pts_ = pts;

    OutputPacket(pkt_ptr);
    return;
}

void TimerSync::HandleAudioPacket(Media_Packet_Ptr pkt_ptr) {
    int64_t dts = pkt_ptr->dts_;
    int64_t pts = pkt_ptr->pts_;

    if (last_audio_dts_ < 0 || last_audio_pts_ < 0) {
        last_audio_dts_ = dts;

        audio_dts_ = 0;
        pkt_ptr->dts_ = audio_dts_;
        pkt_ptr->pts_ = audio_dts_;
        OutputPacket(pkt_ptr);
        return;
    }

    //if dts is reverse
    if (dts <= last_audio_dts_) {
        LogInfof(logger_, "audio packet new dts(%ld) reverse, old dts:%ld", 
                dts, last_audio_dts_);

        audio_dts_ += 1;
        pkt_ptr->dts_ = audio_dts_;
        pkt_ptr->pts_ = audio_dts_;

        last_audio_dts_ = dts;

        OutputPacket(pkt_ptr);
        return;
    }

    int64_t diff_dts = dts - last_audio_dts_;

    audio_dts_ += diff_dts;

    pkt_ptr->dts_ = audio_dts_;
    pkt_ptr->pts_ = audio_dts_;

    last_audio_dts_ = dts;

    OutputPacket(pkt_ptr);
    return;

}

void TimerSync::StartNetwork(const std::string& url, void* loop_handle) {

}

void TimerSync::AddOption(const std::string& key, const std::string& value) {

}

void TimerSync::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}


}

#endif
