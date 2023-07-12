#ifndef JITTER_BUFFER_HPP
#define JITTER_BUFFER_HPP
#include "rtp_packet.hpp"
#include "jitterbuffer_pub.hpp"
#include "logger.hpp"
#include "timer.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <map>
#include <memory>
#include <uv.h>

namespace cpp_streamer
{
#define BUFFER_POOL_SIZE 2048

class JitterBuffer : public TimerInterface
{
public:
    JitterBuffer(MEDIA_PKT_TYPE type, JitterBufferCallbackI* cb, uv_loop_t* loop, Logger* logger);
    ~JitterBuffer();

public:
    void InputRtpPacket(int clock_rate, 
            RtpPacket* input_pkt);

public:
    virtual void OnTimer() override;

private:
    void InitSeq(RtpPacket* input_pkt);
    bool UpdateSeq(RtpPacket* input_pkt, int64_t& extend_seq, bool& reset);
    void OutputPacket(std::shared_ptr<RtpPacketInfo>);
    void CheckTimeout();
    void ReportLost(std::shared_ptr<RtpPacketInfo> pkt_ptr);

private:
    Logger* logger_ = nullptr;
    JitterBufferCallbackI* cb_ = nullptr;
    MEDIA_PKT_TYPE media_type_;
    bool init_flag_ = false;
    uint16_t base_seq_ = 0;
    uint16_t max_seq_  = 0;
    uint32_t bad_seq_  = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
    uint32_t cycles_   = 0;
    std::map<int64_t, std::shared_ptr<RtpPacketInfo>> rtp_packets_map_;//key: extend_seq, value: rtp packet info

private:
    int64_t output_seq_ = 0;
    int64_t report_lost_ts_ = -1;

private:
    uint8_t* pkt_buffers_[BUFFER_POOL_SIZE];
    size_t buffer_index_ = 0;

private:
    int64_t buffer_timeout_ = JITTER_BUFFER_VIDEO_TIMEOUT;
};

}
#endif
