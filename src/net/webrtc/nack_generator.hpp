#ifndef NACK_GENERATOR_HPP
#define NACK_GENERATOR_HPP
#include "rtprtcp_pub.hpp"
#include "rtp_packet.hpp"
#include "timer.hpp"
#include "logger.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <map>

namespace cpp_streamer
{
#define NACK_LIST_MAX        5000
#define NACK_DEFAULT_TIMEOUT 10//ms
#define NACK_RETRY_MAX       20
#define NACK_DEFAULT_RTT     15//ms

class NACK_INFO
{
public:
    NACK_INFO(uint16_t seq, int64_t sent_ms, int retry)
    {
        this->seq     = seq;
        this->sent_ms = sent_ms;
        this->retry   = retry;
    }
    ~NACK_INFO()
    {
    }

public:
    uint16_t seq;
    int64_t sent_ms;
    int retry = 0;
};

class NackGeneratorCallbackI
{
public:
    virtual void GenerateNackList(const std::vector<uint16_t>& seq_vec) = 0;
};

class NackGenerator : public TimerInterface
{
public:
    NackGenerator(uv_loop_t* loop, Logger* logger, NackGeneratorCallbackI* cb);
    virtual ~NackGenerator();

    void UpdateNackList(RtpPacket* pkt);
    void UpdateRtt(int64_t rtt);

protected:
    virtual void OnTimer() override;

private:
    Logger* logger_ = nullptr;

private:
    NackGeneratorCallbackI* cb_ = nullptr;
    bool init_flag_ = false;
    uint16_t last_seq_ = 0;
    std::map<uint16_t, NACK_INFO> nack_map_;
    int64_t rtt_ = NACK_DEFAULT_RTT;
};

}
#endif

