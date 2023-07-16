#ifndef RTC_RECV_STREAM_HPP
#define RTC_RECV_STREAM_HPP
#include "av.hpp"
#include "logger.hpp"
#include "media_packet.hpp"
#include "rtp_packet.hpp"
#include "rtcp_rr.hpp"
#include "rtcp_sr.hpp"
#include "rtcpfb_nack.hpp"
#include "rtcp_xr_dlrr.hpp"
#include "rtcp_xr_rrt.hpp"
#include "rtc_stream_pub.hpp"
#include "stream_statics.hpp"
#include "nack_generator.hpp"

#include <vector>

namespace cpp_streamer
{

class RtcRecvStream : public NackGeneratorCallbackI
{
public:
    RtcRecvStream(MEDIA_PKT_TYPE type, 
            uint32_t ssrc, uint8_t payload, 
            int clock_rate, bool nack, 
            RtcSendStreamCallbackI* cb,
            Logger* logger, uv_loop_t* loop);
    RtcRecvStream(MEDIA_PKT_TYPE type, 
            uint32_t ssrc, uint8_t payload, int clock_rate,
            bool nack, uint8_t rtx_payload, uint32_t rtx_ssrc,
            RtcSendStreamCallbackI* cb,
            Logger* logger, uv_loop_t* loop);
    virtual ~RtcRecvStream();

public:
    void SetSsrc(uint32_t ssrc) { ssrc_ = ssrc; }
    uint32_t GetSsrc() { return ssrc_; }
    void SetPT(uint8_t pt) { pt_ = pt; }
    uint8_t GetPT() { return pt_; }
    void SetClockRate(int clock_rate) { clock_rate_ = clock_rate; }
    int GetClockRate() { return clock_rate_; }

    void SetChannel(int channel) { channel_ = channel; }
    int GetChannel() { return channel_; }

    void SetRtxPT(uint8_t pt) { rtx_payload_ = pt; }
    uint8_t GetRtxPT() { return rtx_payload_; }
    void SetRtxSsrc(uint32_t ssrc) { rtx_ssrc_ = ssrc; }
    uint32_t GetRtxSsrc() { return rtx_ssrc_; }

public:
    void OnTimer(int64_t now_ms);
    RtcpRrBlockInfo* GetRtcpRr(int64_t now_ms);
    void RequestKeyFrame(int64_t now_ms);

public:
    virtual void GenerateNackList(const std::vector<uint16_t>& seq_vec) override;

public:
    void HandleRtpPacket(RtpPacket* pkt);
    void GenerateJitter(uint32_t rtp_timestamp, int64_t recv_pkt_ms);

public:
    void HandleRtcpSr(RtcpSrPacket* pkt);
    void HandleXrRrt(XrRrtData* rrt_block);

public:
    uint32_t GetRtt() { return avg_rtt_; }
    uint32_t GetJitter() { return jitter_; }
    float GetLostRate() { return (float)lost_percent_; }
    void GetStatics(size_t& kbits, size_t& pps);
    int64_t GetResendCount(int64_t now_ms, int64_t& resend_pps);

private:
    void InitSeq(uint16_t seq);
    void UpdateSeq(uint16_t seq);
    int64_t GetExpectedPackets();
    int64_t GetPacketLost();

private:
    Logger* logger_ = nullptr;
    MEDIA_PKT_TYPE media_type_;
    bool nack_enable_ = false;
    bool has_rtx_ = false;

private:
    uint32_t ssrc_  = 0;
    uint8_t pt_     = 0;
    uint16_t seq_   = 0;
    int clock_rate_ = 0;
    int channel_    = 0;

private:
    uint8_t rtx_payload_ = 0;
    uint32_t rtx_ssrc_   = 0;
    uint16_t rtx_seq_    = 0;

private:
    bool first_pkt_ = false;
    uint16_t base_seq_ = 0;
    uint16_t max_seq_  = 0;
    uint32_t bad_seq_  = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
    uint32_t cycles_   = 0;
    int64_t discard_count_ = 0;
    
private://for rtcp sr
    int64_t expect_recv_   = 0;
    int64_t last_recv_     = 0;
    int64_t rtp_timestamp_ = 0;
    int64_t last_sr_ms_    = 0;
    int64_t pkt_count_     = 0;
    int64_t bytes_count_   = 0;
    uint32_t lsr_          = 0;
    uint8_t frac_lost_     = 0;
    double lost_percent_   = 0.0;
    int64_t total_lost_    = 0;

private://for request keyframe
    int64_t last_keyframe_ms_ = -1;
    
private:
    int avg_rtt_ = 0;
    int64_t resend_count_ = 0;
    int64_t last_resend_count_ = 0;
    int64_t last_resend_ms_ = -1;

private://for jitter
    int64_t last_diff_ms_ = 0;
    int64_t jitter_ = 0;

private://for nack
    NackGenerator nack_generator_;

private:
    RtcSendStreamCallbackI* send_cb_ = nullptr;//send rtcp

private:
    StreamStatics statics_;
};


}

#endif
