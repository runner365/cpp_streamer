#include "rtc_recv_stream.hpp"
#include "rtcpfb_nack.hpp"
#include "logger.hpp"

namespace cpp_streamer
{
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
    //LogInfof(logger_, "handle rtp packet:%s", pkt->Dump().c_str());
    uint16_t seq = pkt->GetSeq();

    if (!first_pkt_) {
        first_pkt_ = true;
        InitSeq(seq);
    } else {
        UpdateSeq(seq);
    }

    GenerateJitter(pkt->GetTimestamp(), pkt->GetLocalMs());

    if (nack_enable_) {
        nack_generator_.UpdateNackList(pkt);
    }
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

    send_cb_->SendRtcpPacket(nack_pkt->GetData(), nack_pkt->GetLen());

    delete nack_pkt;
}

void RtcRecvStream::HandleRtcpSr(RtcpSrPacket* sr_pkt) {
    int64_t now_ms = now_millisec();
    NTP_TIMESTAMP ntp_;

    ntp_.ntp_sec   = sr_pkt->GetNtpSec();
    ntp_.ntp_frac  = sr_pkt->GetNtpFrac();
    rtp_timestamp_ = (int64_t)sr_pkt->GetRtpTimestamp();
    pkt_count_     = sr_pkt->GetPktCount();
    bytes_count_   = sr_pkt->GetBytesCount();

    last_sr_ms_ = now_ms;
    lsr_ = ((ntp_.ntp_sec & 0xffff) << 16) | (ntp_.ntp_frac & 0xffff);
}

RtcpRrBlockInfo* RtcRecvStream::GetRtcpRr(int64_t now_ms) {
    RtcpRrBlockInfo* rr_block = new RtcpRrBlockInfo();
    uint32_t highest_seq = (uint32_t)(max_seq_ + cycles_);
    uint32_t dlsr = 0;

    if (last_sr_ms_ > 0) {
        double diff_t = (double)(now_millisec() - last_sr_ms_);
        double dlsr_float = diff_t / 1000 * 65535;

        dlsr = (uint32_t)dlsr_float;
        //log_infof("send_rtcp_rr ssrc:%u, diff_t:%f, dlsr_float:%f, dlsr:%u",
        //    ssrc_, diff_t, dlsr_float, dlsr);
    }
    
    int64_t total_lost = GetPacketLost();

    rr_block->SetReporteeSsrc(ssrc_);
    rr_block->SetFracLost(frac_lost_);
    rr_block->SetCumulativeLost(total_lost);
    rr_block->SetHighestSeq(highest_seq);
    rr_block->SetJitter(jitter_);
    rr_block->SetLsr(lsr_);
    rr_block->SetDlsr(dlsr);

    return rr_block;
}

}

