#ifndef RTC_SEND_STREAM_HPP
#define RTC_SEND_STREAM_HPP
#include "av.hpp"
#include "logger.hpp"
#include "media_packet.hpp"
#include "rtp_packet.hpp"
#include "rtcp_sr.hpp"
#include "rtcp_rr.hpp"
#include "rtcpfb_nack.hpp"
#include "rtcp_xr_dlrr.hpp"
#include "rtc_stream_pub.hpp"
#include "stream_statics.hpp"

#include <vector>

namespace cpp_streamer
{

typedef struct {
    int retry_count;
    int64_t last_ms;
    RtpPacket* pkt;
} SendRtpPacketInfo;

class RtcSendStream
{
public:
    RtcSendStream(MEDIA_PKT_TYPE type, 
            uint32_t ssrc, uint8_t payload, 
            int clock_rate, bool nack, 
            RtcSendStreamCallbackI* cb, Logger* logger);
    RtcSendStream(MEDIA_PKT_TYPE type, 
            uint32_t ssrc, uint8_t payload, int clock_rate,
            bool nack, uint8_t rtx_payload, uint32_t rtx_ssrc,
            RtcSendStreamCallbackI* cb, Logger* logger);
    ~RtcSendStream();

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
    void SendPacket(Media_Packet_Ptr pkt_ptr);
    void OnTimer(int64_t now_ts);

public:
    void HandleRtcpRr(RtcpRrBlockInfo& block_info);
    void HandleRtcpNack(RtcpFbNack* nack_pkt);
    void HandleXrDlrr(XrDlrrData* dlrr_block);

public:
    uint32_t GetRtt() { return avg_rtt_; }
    uint32_t GetJitter() { return jitter_; }
    float GetLostRate() { return lost_rate_; }
    void GetStatics(size_t& kbits, size_t& pps);
    int64_t GetResendCount(int64_t now_ms, int64_t& resend_pps);

private:
    void SendVideoPacket(Media_Packet_Ptr pkt_ptr);
    void SendAudioPacket(Media_Packet_Ptr pkt_ptr);

private:
    void SendH264Packet(Media_Packet_Ptr pkt_ptr);

private:
    void SendVideoRtpPacket(RtpPacket* pkt, bool resend = false);
    void SendAudioRtpPacket(RtpPacket* pkt);
    void SaveBuffer(RtpPacket* pkt);
    void ResendRtpPacket(uint16_t seq);

private:
    RtcpSrPacket* GetRtcpSr(int64_t now_ms);

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
    RtcSendStreamCallbackI* cb_ = nullptr;

private:
    uint8_t sps_[512];
    uint8_t pps_[512];
    int sps_len_ = 0;
    int pps_len_ = 0;

private:
    std::vector<SendRtpPacketInfo> send_buffer_;

private://for rtcp sr
    NTP_TIMESTAMP last_sr_ntp_ts_;
    uint32_t last_sr_rtp_ts_ = 0;
    uint32_t sent_count_ = 0;
    uint32_t sent_bytes_ = 0;
    int64_t  last_sr_ts_ = 0;//interval 500ms

private:
    uint32_t jitter_     = 0;
    uint32_t lost_total_ = 0;
    float lost_rate_     = 0.0;
    float rtt_           = (float)RTT_DEFAULT;
    float avg_rtt_       = (float)RTT_DEFAULT;
    int64_t resend_cnt_  = 0;
    int64_t last_resend_cnt_        = 0;
    int64_t last_resend_statics_ms_ = 0;

private:
    StreamStatics statics_;
};

}

#endif

