#include "rtc_recv_stream.hpp"
#include "logger.hpp"

namespace cpp_streamer
{
RtcRecvStream::RtcRecvStream(MEDIA_PKT_TYPE type, 
        uint32_t ssrc, uint8_t payload, 
        int clock_rate, bool nack, 
        Logger* logger, uv_loop_t* loop):logger_(logger)
                                         , media_type_(type)
                                         , nack_enable_(nack)
                                         , ssrc_(ssrc)
                                         , pt_(payload)
                                         , clock_rate_(clock_rate)
                                         , nack_generator_(loop, logger, this)
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
        Logger* logger, uv_loop_t* loop) :logger_(logger)
                                          , media_type_(type)
                                          , nack_enable_(nack)
                                          , ssrc_(ssrc)
                                          , pt_(payload)
                                          , clock_rate_(clock_rate)
                                          , nack_generator_(loop, logger, this)
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

void RtcRecvStream::GenerateNackList(const std::vector<uint16_t>& seq_vec) {

}

}

