#ifndef WAIT_BASED_ON_TIMESTAMP
#define WAIT_BASED_ON_TIMESTAMP
#include "media_packet.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <thread>
#include <iostream>

namespace cpp_streamer
{

#define PACKET_DIFF_MAX_MS (5 * 1000)

class WaitBasedOnTimestamp
{
public:
    WaitBasedOnTimestamp()
    {
    }
    ~WaitBasedOnTimestamp()
    {
    }

public:
    void Wait(Media_Packet_Ptr pkt_ptr) {
        int64_t now_ms = now_millisec();
        int64_t pkt_ts = pkt_ptr->dts_;

        if (first_pkt_ts_ <= 0) {
            first_pkt_ts_ = pkt_ts;
            last_pkt_ts_  = pkt_ts;
            first_sys_ts_ = now_ms;
            return;
        }

        int64_t diff_t = (pkt_ts > last_pkt_ts_) ? (pkt_ts - last_pkt_ts_) : (last_pkt_ts_ - pkt_ts);
        last_pkt_ts_  = pkt_ts;

        if (diff_t > PACKET_DIFF_MAX_MS) {
            first_pkt_ts_ = pkt_ts;
            last_pkt_ts_  = pkt_ts;
            first_sys_ts_ = now_ms;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return;
        }

        diff_t = (now_ms - first_sys_ts_) - (pkt_ts - first_pkt_ts_);
        while(diff_t < 30) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            now_ms = now_millisec();
            diff_t = (now_ms - first_sys_ts_) - (pkt_ts - first_pkt_ts_);
        }
        return;
    }

    void Reset() {
        first_pkt_ts_ = -1;
        first_sys_ts_ = -1;
        last_pkt_ts_ = -1;
    }

private:
    int64_t first_pkt_ts_ = -1;
    int64_t first_sys_ts_ = -1;
    int64_t last_pkt_ts_ = -1;
};

}
#endif
