#ifndef H264_PACK_HANDLE_HPP
#define H264_PACK_HANDLE_HPP
#include "pack_handle_pub.hpp"
#include "rtp_packet.hpp"
#include "timer.hpp"
#include "logger.hpp"
#include <queue>

namespace cpp_streamer
{
class PackHandleH264 : public PackHandleBase, public TimerInterface
{
public:
    PackHandleH264(PackCallbackI* cb, uv_loop_t* io_ctx, Logger* logger);
    virtual ~PackHandleH264();

public:
    virtual void InputRtpPacket(std::shared_ptr<RtpPacketInfo> pkt_ptr) override;

public:
    virtual void OnTimer() override;
    
private:
    void GetStartEndBit(RtpPacket* pkt, bool& start, bool& end);
    void ResetRtpFua();
    bool DemuxFua(Media_Packet_Ptr h264_pkt_ptr, int64_t& timestamp);
    bool DemuxStapA(std::shared_ptr<RtpPacketInfo>);
    bool ParseStapAOffsets(const uint8_t* data, size_t data_len, std::vector<size_t> &offsets);
    void CheckFuaTimeout();
    void ReportLost(std::shared_ptr<RtpPacketInfo> pkt_ptr);
    
private:
    bool init_flag_  = false;
    bool start_flag_ = false;
    bool end_flag_   = false;
    int64_t last_extend_seq_ = 0;
    std::deque<std::shared_ptr<RtpPacketInfo>> packets_queue_;
    PackCallbackI* cb_ = nullptr;
    int64_t report_lost_ts_ = -1;

private:
    Logger* logger_ = nullptr;
};
}

#endif
