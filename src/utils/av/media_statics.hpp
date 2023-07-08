#ifndef MEDIA_STATICS_HPP
#define MEDIA_STATICS_HPP
#include "av.hpp"
#include "media_packet.hpp"
#include "timeex.hpp"
#include <iostream>

namespace cpp_streamer
{
#define MEDIA_STATICS_DEF_INTERVAL 2000

class MediaStatics
{
public:
    MediaStatics(int64_t interval):interval_(interval)
    {
        interval_ = (interval < MEDIA_STATICS_DEF_INTERVAL) ? MEDIA_STATICS_DEF_INTERVAL : interval;
    }
    ~MediaStatics()
    {
    }

public:
    int InputPacket(Media_Packet_Ptr pkt_ptr, bool basedOnSysTime = true) {
        if (!pkt_ptr) {
            return 0;
        }

        if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
            return HandleVideo(pkt_ptr, basedOnSysTime);
        }

        if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
            return HandleAudio(pkt_ptr, basedOnSysTime);
        }

        return 0;
    }

    int64_t GetVideoKbitRate() {
        return video_kbits_sec_;
    }

    int64_t GetVideoFrameRate() {
        return video_frames_sec_;
    }

    int64_t GetAudioKbitRate() {
        return audio_kbits_sec_;
    }

    int64_t GetAudioFrameRate() {
        return audio_frames_sec_;
    }

    int GetGop() {
        return gop_;
    }

private:
    int HandleVideo(Media_Packet_Ptr pkt_ptr, bool basedOnSysTime) {
        int64_t current_ts_ = basedOnSysTime ? now_millisec() : pkt_ptr->dts_;

        video_bytes_ += pkt_ptr->buffer_ptr_->DataLen();
        video_frames_++;
        gop_frames_++;
        
        if (pkt_ptr->is_key_frame_) {
            gop_ = gop_frames_;
            gop_frames_ = 0;
        }

        if (last_video_ts_ < 0) {
            last_video_ts_ = current_ts_;
            return 0;
        }

        if ((current_ts_ < last_video_ts_) && (last_video_ts_ - current_ts_) > interval_){
            last_video_ts_ = current_ts_;
            return 0;
        }

        int64_t diff_t = (current_ts_ > last_video_ts_) ? (current_ts_ - last_video_ts_) : (last_video_ts_ - current_ts_);
        if (diff_t >= interval_) {
            int64_t bytes = video_bytes_ - last_video_bytes_;
            int64_t frames = video_frames_ - last_video_frames_;

            video_kbits_sec_ = bytes * 8 / diff_t;
            video_frames_sec_ = frames / (diff_t / 1000);

            last_video_bytes_ = video_bytes_;
            last_video_frames_ = video_frames_;
            last_video_ts_ = current_ts_;
            return 1;
        }
        return 0;
    }

    int HandleAudio(Media_Packet_Ptr pkt_ptr, bool basedOnSysTime) {
        int64_t current_ts_ = basedOnSysTime ? now_millisec() : pkt_ptr->dts_;

        audio_bytes_ += pkt_ptr->buffer_ptr_->DataLen();
        audio_frames_++;

        if (last_audio_ts_ < 0) {
            last_audio_ts_ = current_ts_;
            return 0;
        }

        if ((current_ts_ < last_audio_ts_) && (last_audio_ts_ - current_ts_) > interval_){
            last_audio_ts_ = current_ts_;
            return 0;
        }

        int64_t diff_t = (current_ts_ > last_audio_ts_) ? (current_ts_ - last_audio_ts_) : (last_audio_ts_ - current_ts_);
        if (diff_t >= interval_) {
            int64_t bytes = audio_bytes_ - last_audio_bytes_;
            int64_t frames = audio_frames_ - last_audio_frames_;

            audio_kbits_sec_ = bytes * 8 / diff_t;
            audio_frames_sec_ = frames / (diff_t / 1000);

            last_audio_bytes_ = audio_bytes_;
            last_audio_frames_ = audio_frames_;
            last_audio_ts_ = current_ts_;
            return 1;
        }
        return 0;
    }

private:
    int64_t interval_ = 2000;//ms

    int64_t last_video_ts_     = -1;
    int64_t video_bytes_       = 0;
    int64_t video_frames_      = 0;
    int64_t last_video_bytes_  = 0;
    int64_t last_video_frames_ = 0;
    int64_t video_kbits_sec_   = 0;
    int64_t video_frames_sec_  = 0;
    int gop_frames_            = 0;
    int gop_                   = 0;
    
    int64_t last_audio_ts_     = -1;
    int64_t audio_bytes_       = 0;
    int64_t audio_frames_      = 0;
    int64_t last_audio_bytes_  = 0;
    int64_t last_audio_frames_ = 0;
    int64_t audio_kbits_sec_   = 0;
    int64_t audio_frames_sec_  = 0;
};

}

#endif
