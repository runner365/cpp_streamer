#include "rtc_recv_stream.hpp"
#include "rtcpfb_nack.hpp"
#include "rtcp_pspli.hpp"
#include "logger.hpp"

namespace cpp_streamer
{
#define REQ_KEYFRAME_INTERVAL (5*1000)

RtcRecvStream::RtcRecvStream(MEDIA_PKT_TYPE type, 
        uint32_t ssrc, uint8_t payload, 
        int clock_rate, bool nack, 
        RtcSendStreamCallbackI* cb,
        Logger* logger, uv_loop_t* loop):logger_(logger)
                                         , media_type_(type)
                                         , nack_enable_(nack)
                                         , ssrc_(ssrc)
                                         , pt_(payload)
                                         , clock_rate_(clock_rate)
                                         , nack_generator_(loop, logger, this)
                                         , send_cb_(cb)
{
    has_rtx_ = false;
    LogInfof(logger_, "RtcRecvStream construct type:%s, ssrc:%u, payload:%d, clock rate:%d, nack:%s, rtx:disable",
            (type == MEDIA_VIDEO_TYPE) ? "video" : "audio",
            ssrc, payload, clock_rate,
            nack ? "enable" : "disable");
}

RtcRecvStream::RtcRecvStream(MEDIA_PKT_TYPE type, 
        uint32_t ssrc, uint8_t payload, int clock_rate,
        bool nack, uint8_t rtx_payload, uint32_t rtx_ssrc,
        RtcSendStreamCallbackI* cb,
        Logger* logger, uv_loop_t* loop) :logger_(logger)
                                          , media_type_(type)
                                          , nack_enable_(nack)
                                          , ssrc_(ssrc)
                                          , pt_(payload)
                                          , clock_rate_(clock_rate)
                                          , nack_generator_(loop, logger, this)
                                          , send_cb_(cb)
{
    has_rtx_ = true;
    rtx_payload_ = rtx_payload;
    rtx_ssrc_ = rtx_ssrc;

    LogInfof(logger_, "RtcRecvStream construct type:%s, ssrc:%u, payload:%d, clock rate:%d, nack:%s, rtx:enable, rtx ssrc:%u, rtx payload:%d",
            (type == MEDIA_VIDEO_TYPE) ? "video" : "audio",
            ssrc, payload, clock_rate,
            nack ? "enable" : "disable",
            rtx_ssrc_, rtx_payload_);
}

RtcRecvStream::~RtcRecvStream() {
    LogInfof(logger_, "RtcRecvStream destruct type:%s",
            (media_type_ == MEDIA_VIDEO_TYPE) ? "video" : "audio");
}

void RtcRecvStream::GenerateJitter(uint32_t rtp_timestamp, int64_t recv_pkt_ms) {
    if (clock_rate_ <= 0) {
        CSM_THROW_ERROR("clock rate(%d) is invalid", clock_rate_);
    }
    int64_t rtp_ms = rtp_timestamp * 1000 / clock_rate_;
    int64_t diff_t = recv_pkt_ms - rtp_ms;

    if (last_diff_ms_ == 0) {
        last_diff_ms_ = diff_t;
        return;
    }

    int64_t d = diff_t - last_diff_ms_;
    d = (d < 0) ? (-d) : d;

    jitter_ += (d - jitter_)/8.0;

    return;
}

void RtcRecvStream::HandleRtpPacket(RtpPacket* pkt) {
    uint32_t ssrc = pkt->GetSsrc();
    uint16_t seq = pkt->GetSeq();

    if (ssrc == GetRtxSsrc()) {
        //LogInfof(logger_, "handle rtx packet:%s", pkt->Dump().c_str());
        pkt->RtxDemux(GetSsrc(), GetPT());
        //LogInfof(logger_, "handle rtx recover packet:%s", pkt->Dump().c_str());
        //LogInfof(logger_, "handle rtx recover packet seq:%d, rtx seq:%d", pkt->GetSeq(), seq);

        ssrc = pkt->GetSsrc();
        seq  = pkt->GetSeq();
    } else {
        GenerateJitter(pkt->GetTimestamp(), pkt->GetLocalMs());
    }

    if (!first_pkt_) {
        first_pkt_ = true;
        InitSeq(seq);
    } else {
        UpdateSeq(seq);
    }

    if (nack_enable_) {
        nack_generator_.UpdateNackList(pkt);
    }

    statics_.Update(pkt->GetDataLength(), pkt->GetLocalMs()); 
}

//rfc3550: A.1 RTP Data Header Validity Checks
void RtcRecvStream::InitSeq(uint16_t seq) {
    base_seq_ = seq;
    max_seq_  = seq;
    bad_seq_  = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
    cycles_   = 0;
}

//rfc3550: A.1 RTP Data Header Validity Checks
void RtcRecvStream::UpdateSeq(uint16_t seq) {
    const int MAX_DROPOUT    = 3000;
    const int MAX_MISORDER   = 100;
    //const int MIN_SEQUENTIAL = 2;

    uint16_t udelta = seq - max_seq_;

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
               InitSeq(seq);
           }
           else {
               bad_seq_= (seq + 1) & (RTP_SEQ_MOD-1);
               discard_count_++;
               return;
           }
    } else {
        /* duplicate or reordered packet */
    }
}

int64_t RtcRecvStream::GetExpectedPackets() {
    return cycles_ + max_seq_ - bad_seq_ + 1;
}

int64_t RtcRecvStream::GetPacketLost() {
    int64_t expected = GetExpectedPackets();
    int64_t recv_count = (int64_t)statics_.GetCount();

    int64_t expected_interval = expected - expect_recv_;
    expect_recv_ = expected;

    int64_t recv_interval = recv_count - last_recv_;
    if (last_recv_ <= 0) {
        last_recv_ = recv_count;
        return 0;
    }
    last_recv_ = recv_count;

    if ((expected_interval <= 0) || (recv_interval <= 0)) {
        frac_lost_ = 0;
    } else {
        //log_infof("expected_interval:%ld, recv_interval:%ld, ssrc:%u, media:%s",
        //    expected_interval, recv_interval, ssrc_, media_type_.c_str());
        total_lost_ += expected_interval - recv_interval;
        frac_lost_ = std::round((double)((expected_interval - recv_interval) * 256) / expected_interval);
        lost_percent_ = (expected_interval - recv_interval) / expected_interval;
    }

    return total_lost_;
}

void RtcRecvStream::GenerateNackList(const std::vector<uint16_t>& seq_vec) {
    RtcpFbNack* nack_pkt = new RtcpFbNack(0, ssrc_);
    nack_pkt->InsertSeqList(seq_vec);

    resend_count_ += seq_vec.size();
    send_cb_->SendRtcpPacket(nack_pkt->GetData(), nack_pkt->GetLen());

    delete nack_pkt;
}

void RtcRecvStream::HandleXrDlrr(XrDlrrData* dlrr_block) {
    uint32_t dlrr  = ntohl(dlrr_block->dlrr);
    uint32_t lrr   = ntohl(dlrr_block->lrr);
    int64_t now_ms = now_millisec();
    NTP_TIMESTAMP ntp = millisec_to_ntp(now_ms);
    uint32_t compound_now = 0;

    compound_now |= (ntp.ntp_sec & 0xffff) << 16;
    compound_now |= (ntp.ntp_frac & 0xffff0000) >> 16;
    
    if (compound_now < (dlrr + lrr)) {
        return;
    }
    uint32_t rtt = compound_now - dlrr - lrr;
    float rtt_float = ((rtt & 0xffff0000) >> 16) * 1000.0;
    rtt_float += ((rtt & 0xffff) / 65536.0) * 1000.0;


    avg_rtt_ += ((int64_t)rtt_float - avg_rtt_)/5;

    return;
}

void RtcRecvStream::HandleRtcpSr(RtcpSrPacket* sr_pkt) {
    int64_t now_ms = now_millisec();
    NTP_TIMESTAMP ntp;

    ntp.ntp_sec   = sr_pkt->GetNtpSec();
    ntp.ntp_frac  = sr_pkt->GetNtpFrac();
    rtp_timestamp_ = (int64_t)sr_pkt->GetRtpTimestamp();
    pkt_count_     = sr_pkt->GetPktCount();
    bytes_count_   = sr_pkt->GetBytesCount();

    last_sr_ms_ = now_ms;
    lsr_ = ((ntp.ntp_sec & 0xffff) << 16) | ((ntp.ntp_frac >> 16) & 0xffff);
    LogDebugf(logger_, "rtc recv stream media type:%d, ntp:%u.%u, rtp ts:%ld, pkt count:%u, bytes:%u",
            media_type_, ntp.ntp_sec, ntp.ntp_frac, rtp_timestamp_, pkt_count_, bytes_count_);
}

RtcpRrBlockInfo* RtcRecvStream::GetRtcpRr(int64_t now_ms) {
    RtcpRrBlockInfo* rr_block = new RtcpRrBlockInfo();
    uint32_t highest_seq = (uint32_t)(max_seq_ + cycles_);
    uint32_t dlsr = 0;

    if (last_sr_ms_ > 0) {
        uint32_t diff_t = (uint32_t)(now_millisec() - last_sr_ms_);
        dlsr = (diff_t / 1000) << 16;
        dlsr |= (uint32_t)((diff_t % 1000) * 65536 / 1000);
    }
    int64_t total_lost = GetPacketLost();
    rr_block->SetReporteeSsrc(ssrc_);
    rr_block->SetFracLost(frac_lost_);
    rr_block->SetCumulativeLost(total_lost);
    rr_block->SetHighestSeq(highest_seq);
    rr_block->SetJitter(jitter_);
    rr_block->SetLsr(lsr_);
    rr_block->SetDlsr(dlsr);

    LogDebugf(logger_, "send_rtcp_rr ssrc:%u, lsr:%u, dlsr:%u, frac lost:%d, total lost:%d",
            ssrc_, lsr_, dlsr, frac_lost_, total_lost);
    return rr_block;
}

void RtcRecvStream::OnTimer(int64_t now_ms) {
    SendXrRrt(now_ms);
    RequestKeyFrame(now_ms);
}

void RtcRecvStream::SendXrRrt(int64_t now_ms) {
    XrRrt rrt;
    NTP_TIMESTAMP ntp_now = millisec_to_ntp(now_ms);
    rrt.SetSsrc(ssrc_);
    rrt.SetNtp(ntp_now.ntp_sec, ntp_now.ntp_frac);
    
    send_cb_->SendRtcpPacket(rrt.GetData(), rrt.GetDataLen());
}

void RtcRecvStream::RequestKeyFrame(int64_t now_ms) {
    if (media_type_ != MEDIA_VIDEO_TYPE) {
        LogErrorf(logger_, "only video request keyframe.");
        return;
    }

    if (now_ms > 0) {
        if (last_keyframe_ms_ <= 0) {
            last_keyframe_ms_ = now_ms;
        } else {
            int64_t diff_t = now_ms - last_keyframe_ms_;
            if (diff_t < REQ_KEYFRAME_INTERVAL) {
                return;
            }
            last_keyframe_ms_ = now_ms;
        }
    } else {
        last_keyframe_ms_ = now_millisec();
    }

    RtcpPsPli* pspli_pkt = new RtcpPsPli();

    pspli_pkt->SetSenderSsrc(1);
    pspli_pkt->SetMediaSsrc(ssrc_);

    //LogInfof(logger_, "request frame:%s", pspli_pkt->Dump().c_str());
    send_cb_->SendRtcpPacket(pspli_pkt->GetData(), pspli_pkt->GetDataLen());

    delete pspli_pkt;
}

void RtcRecvStream::GetStatics(size_t& kbits, size_t& pps) {
    int64_t now_ms = now_millisec();

    kbits = statics_.BytesPerSecond(now_ms, pps) * 8 / 1000;
}


int64_t RtcRecvStream::GetResendCount(int64_t now_ms, int64_t& resend_pps) {
    if (last_resend_ms_ <= 0) {
        last_resend_ms_ = now_ms;
        last_resend_count_ = resend_count_;
        return resend_count_;
    }

    int64_t diff_t = now_ms - last_resend_ms_;

    if (diff_t <= 0) {
        return resend_count_;
    }
    int64_t diff_count = resend_count_ - last_resend_count_;
    resend_pps = diff_count * 1000 / diff_t;

    last_resend_ms_ = now_ms;
    last_resend_count_ = resend_count_;

    return resend_count_;
}

}

