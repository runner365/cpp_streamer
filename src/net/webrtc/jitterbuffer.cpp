#include "jitterbuffer.hpp"
#include "timeex.hpp"
#include "logger.hpp"

namespace cpp_streamer
{
JitterBuffer::JitterBuffer(MEDIA_PKT_TYPE type,
        JitterBufferCallbackI* cb, 
        uv_loop_t* loop, 
        Logger* logger):TimerInterface(loop, 100)
                       , logger_(logger)
                       , cb_(cb) 
                       , media_type_(type){
    for (size_t i = 0; i < BUFFER_POOL_SIZE; i++) {
        pkt_buffers_[i] = new uint8_t[RTP_PACKET_MAX_SIZE];
    }
    if (type == MEDIA_VIDEO_TYPE) {
        buffer_timeout_ = JITTER_BUFFER_VIDEO_TIMEOUT;
    } else if (type == MEDIA_AUDIO_TYPE) {
        buffer_timeout_ = JITTER_BUFFER_AUDIO_TIMEOUT;
    } else {
        CSM_THROW_ERROR("JItterBuffer construct media_type %d error",
                type);
    }
}

JitterBuffer::~JitterBuffer() {
    for (size_t i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pkt_buffers_[i]) {
            delete[] pkt_buffers_[i];
        }
    }
}

void JitterBuffer::InputRtpPacket(int clock_rate, 
            RtpPacket* pkt) {
    int64_t extend_seq = 0;
    bool reset = false;
    bool first_pkt = false;
    size_t index = (buffer_index_++) % RTP_PACKET_MAX_SIZE;
    uint8_t* buffer = pkt_buffers_[index];

    RtpPacket* input_pkt = pkt->Clone(buffer);

    if (!init_flag_) {
        init_flag_ = true;
        InitSeq(input_pkt);
        
        reset = true;
        first_pkt = true;
        extend_seq = input_pkt->GetSeq();
    } else {
        bool bad_pkt = UpdateSeq(input_pkt, extend_seq, reset);
        if (!bad_pkt) {
            return;
        }
        if (reset) {
            init_flag_ = false;
        }
    }

    auto pkt_info_ptr = std::make_shared<RtpPacketInfo>(media_type_,
                                                        clock_rate,
                                                        input_pkt,
                                                        extend_seq);
    if (reset) {
        //if the rtc client is reset, call the reset callback which send pli
        ReportLost(pkt_info_ptr);
    }

    //if it's the first packet, output the packet
    if (first_pkt || reset) {
        OutputPacket(pkt_info_ptr);
        return;
    }

    //if the seq is continued, output the packet
    if ((output_seq_ + 1) == extend_seq) {
        OutputPacket(pkt_info_ptr);

        //check the packet in map
        for (auto iter = rtp_packets_map_.begin();
            iter != rtp_packets_map_.end();
            ) {
            int64_t pkt_extend_seq = iter->first;
            if ((output_seq_ + 1) == pkt_extend_seq) {
                if (iter->second->media_type_ == MEDIA_VIDEO_TYPE) {
                    LogDebugf(logger_, "jitter buffer media type:%d, output seq(%d) in buffer queue",
                        iter->second->media_type_, pkt_extend_seq);
                }
                OutputPacket(iter->second);
                iter = rtp_packets_map_.erase(iter);
                continue;
            }
            break;
        }
        return;
    } else if (extend_seq <= output_seq_) {
        LogInfof(logger_, "receive old seq:%ld, output_seq:%ld media type:%d",
                extend_seq, output_seq_, pkt_info_ptr->media_type_);
        return;
    }
    rtp_packets_map_[extend_seq] = pkt_info_ptr;
    if (pkt_info_ptr->media_type_ == MEDIA_VIDEO_TYPE) {
        LogDebugf(logger_, "JitterBuffer media type:%d, packets queue len:%lu, pkt seq:%d, last output seq:%d",
            pkt_info_ptr->media_type_, rtp_packets_map_.size(), pkt_info_ptr->extend_seq_, output_seq_);
    }

    CheckTimeout();

    return;
}

void JitterBuffer::OnTimer() {
    CheckTimeout();
}

void JitterBuffer::CheckTimeout() {
    if (rtp_packets_map_.empty()) {
        return;
    }
    int64_t now_ms = now_millisec();
    for(auto iter = rtp_packets_map_.begin();
        iter != rtp_packets_map_.end();) {
        std::shared_ptr<RtpPacketInfo> pkt_info_ptr = iter->second;
        int64_t diff_t = now_ms - pkt_info_ptr->pkt->GetLocalMs();

        if (diff_t > buffer_timeout_) {
            if (pkt_info_ptr->media_type_ == MEDIA_VIDEO_TYPE) {
                LogInfof(logger_, "timeout output type:%d, seq:%d, timeout:%ld",
                    pkt_info_ptr->media_type_, pkt_info_ptr->extend_seq_, buffer_timeout_);
            }

            OutputPacket(pkt_info_ptr);
            iter = rtp_packets_map_.erase(iter);
            ReportLost(pkt_info_ptr);
            continue;
        }
        if ((output_seq_ + 1) == pkt_info_ptr->extend_seq_) {
            OutputPacket(iter->second);
            iter = rtp_packets_map_.erase(iter);
            continue;
        }
        iter++;
    }

    return;
}

void JitterBuffer::ReportLost(std::shared_ptr<RtpPacketInfo> pkt_ptr) {
    int64_t now_ms = now_millisec();

    if (now_ms - report_lost_ts_ > 500) {
        report_lost_ts_ = now_ms;
        cb_->RtpPacketReset(pkt_ptr);
    }
    
}

void JitterBuffer::OutputPacket(std::shared_ptr<RtpPacketInfo> pkt_ptr) {
    output_seq_ = pkt_ptr->extend_seq_;
    cb_->RtpPacketOutput(pkt_ptr);
    return;
}

void JitterBuffer::InitSeq(RtpPacket* input_pkt) {
    base_seq_ = input_pkt->GetSeq();
    max_seq_  = input_pkt->GetSeq();
    bad_seq_  = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
    cycles_   = 0;
}

bool JitterBuffer::UpdateSeq(RtpPacket* input_pkt, int64_t& extend_seq, bool& reset) {
    const int MAX_DROPOUT    = 3000;
    const int MAX_MISORDER   = 100;
    uint16_t seq = input_pkt->GetSeq();
    //const int MIN_SEQUENTIAL = 2;

    uint16_t udelta = seq - max_seq_;

    reset = false;

    if (udelta < MAX_DROPOUT) {
        /* in order, with permissible gap */
        if (seq < max_seq_) {
            /*
             * Sequence number wrapped - count another 64K cycle.
             */
            cycles_ += RTP_SEQ_MOD;
        }
        max_seq_ = seq;
    } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
            /* the sequence number made a very large jump */
            if (seq == bad_seq_) {
                /*
                 * Two sequential packets -- assume that the other side
                 * restarted without telling us so just re-sync
                 * (i.e., pretend this was the first packet).
                 */
                InitSeq(input_pkt);
                reset = true;
                extend_seq = cycles_ + seq;
                return true;
            } else {
                bad_seq_= (seq + 1) & (RTP_SEQ_MOD-1);
                return false;
            }
    } else {
        /* duplicate or reordered packet */
    }
    extend_seq = cycles_ + seq;
    return true;
}

}
